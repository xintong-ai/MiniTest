// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2008-2014 NVIDIA Corporation. All rights reserved.

// Program description:
// This program demonstrates an APEX Emitter with an 'explicit' geometry, meaning that the
// application specifies the particle spawn positions and velocities.  This emitter uses
// a BasicIOS for particle simulation and a sprite IOFX for "rendering".  The rendering
// consists of printing the particle positions to STDOUT.
//
// Scene description:
// A turbulence grid is placed in the world at the bottom just 1 unit above the origin
// in the (y-direction).  Emitted particles move up freely for one frame, then begin
// to slow once they are in the grid.
//
// Command line options:
// To run the program with no turbulence, pass 'noTurbulence' on the command line.
//
// Prerequisites: 
// This program is intended to work on windows with PhysX 3.x

#include <cstdio>
#include <cstddef>

// PhysX includes
#include <PxPhysics.h>
#include <PxSceneDesc.h>
#include <PxScene.h>
#include <common/PxTolerancesScale.h>
#include <cooking/PxCooking.h>
#include <pxtask/PxCudaContextManager.h>
#include <extensions/PxDefaultCpuDispatcher.h>
#include <extensions/PxDefaultSimulationFilterShader.h>
#include <foundation/PxFoundation.h>
#include <foundation/PxAllocatorCallback.h>
#include <foundation/PxErrorCallback.h>
#include <foundation/PxBounds3.h>

// APEX includes
#include <NxApex.h>
#include <NxApexSDK.h>
#include <PxFileBuf.h>
#include <NxParameterized.h>
#include <NxParamUtils.h>
//#include <NxBasicIosAsset.h>
#include <NxApexEmitterAsset.h>
#include <NxApexEmitterActor.h>
//#include <NxModuleParticles.h>
//#include <NxModuleIofx.h>
//#include <NxIofxActor.h>
#include <NxIofxAsset.h>
//#include <NxTurbulenceFSAsset.h>
//#include <NxTurbulenceFSActor.h>
//#include <NxApexRenderVolume.h>

// This program only works on windows with PhysX 3.2
#if defined(PX_WINDOWS) && NX_SDK_VERSION_MAJOR == 3

// Needed for the WinMain
#include <windows.h>

// Utility includes
#include <string>
#include <list>

// a small helper method for all those times we need to release and clear
template <class T>
static void releaseAndClear(T*& resource)
{
	if (resource != NULL)
	{
		resource->release();
		resource = NULL;
	}
}

// a utility to change the text color in the console
class ConsoleTextColor
{
public:
	ConsoleTextColor( WORD color )
	{
		CONSOLE_SCREEN_BUFFER_INFO info;
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
		previousColor = info.wAttributes;
		setConsoleTextColor(color);
	}

	~ConsoleTextColor()
	{
		setConsoleTextColor(previousColor);
	}

	static void setConsoleTextColor( WORD color )
	{
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
	}

private:
	ConsoleTextColor(){}

	WORD previousColor;
};
	
// these help us find the APEX media for both the internal source repository, in a distribution,
// and in future revisions of the APEX SDK
#define QUICK_STRINGIZE_HELPER(X)	#X
#define QUICK_STRINGIZE(X)			QUICK_STRINGIZE_HELPER(X)
const char* ASSET_PATH_UNDER_MEDIA	= "/"QUICK_STRINGIZE(MEDIA_APEX);


using namespace physx;
using namespace physx::apex;
using namespace physx::general_PxIOStream2;

// An allocator callback for APEX and PhysX
class AppAlloc : public PxAllocatorCallback
{
	// PhysX3 PxAllocatorCallback interface
	void* allocate(size_t size, const char* /*typeName*/, const char* /*filename*/, int /*line*/)
	{
		return ::_aligned_malloc(size, 16);
	}

	void deallocate(void* ptr)
	{
		return ::_aligned_free(ptr);
	}
};

// An error callback for APEX and PhysX
class AppErrorCallback : public PxErrorCallback
{
	// PhysX3 foundation PxErrorCallback interface
	void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line)
	{
		printf("ErrorCallback in file: %s(%i):\n%s\n", file, line, message);
	}

};

class DummyMaterial
{};

// A resource callback for APEX (APEX will ask for assets, materials, etc through this interface)
class AppApexResourceCallback : public NxResourceCallback
{
public:
	AppApexResourceCallback::AppApexResourceCallback()
		: mApexSDK(NULL)
	{
		mPathToMedia[0] = 0;
	}

	void setApexSDK(NxApexSDK * apexSDK)
	{
		mApexSDK = apexSDK;
	}

	// This method is intended to find the specified path (in this case "media") in 
	// a folder above the current folder.
	// It will typically leave something like "../../../../../media" in the outBuffer
	bool searchForPath(const char* path, char* outBuffer, int outBufferSize, int maxRecursion)
	{
		char* tmpBuffer = (char*)alloca(outBufferSize);
		strcpy_s(outBuffer, outBufferSize, path);
		for(int i = 0; i < maxRecursion; i++)
		{
			if(GetFileAttributes(outBuffer) == INVALID_FILE_ATTRIBUTES)
			{
				sprintf_s(tmpBuffer, outBufferSize, "../%s", outBuffer);
				strcpy_s(outBuffer, outBufferSize, tmpBuffer);
			}
			else 
				return true;
		}
		return false;
	}

	// This method is called by APEX to retrieve resources
	void* requestResource(const char* nameSpace, const char* name)
	{
		ConsoleTextColor consoleColor(FOREGROUND_GREEN);
		if(!mApexSDK)
		{
			return NULL;
		}

		printf("requestResource called for <%s> %s\n", nameSpace, name);

		if (!strcmp(nameSpace, APEX_MATERIALS_NAME_SPACE))
		{
			return &material;
		}
		// These strings should be pulled from the module headers
		else if (!strcmp(nameSpace, NX_APEX_EMITTER_AUTHORING_TYPE_NAME) ||
			!strcmp(nameSpace, NX_IOFX_AUTHORING_TYPE_NAME) ||
			!strcmp(nameSpace, NX_BASIC_IOS_AUTHORING_TYPE_NAME) ||
			!strcmp(nameSpace, NX_TURBULENCE_FS_AUTHORING_TYPE_NAME))
		{
			// find the path to the APEX media folder
			if (mPathToMedia[0] == 0)
			{
				if (!searchForPath("media", mPathToMedia, sizeof(mPathToMedia), 20))
				{
					printf("Error: requestResources cannot find the media folder\n");
					return NULL;
				}
			}

			// make a file stream for the asset file
			std::string filename;
			filename += mPathToMedia;
			filename += ASSET_PATH_UNDER_MEDIA;
			filename += "/MinimalTurbulence/";
			filename += name;
			filename += ".apx";
			PxFileBuf* fileStream = mApexSDK->createStream(filename.c_str(), PxFileBuf::OPEN_READ_ONLY);
			if(!fileStream->isOpen())
			{
				printf("Error: requestResources failed to open %s\n", filename.c_str());
				return NULL;
			}

			// deserialize the asset into an NxParameterized object
			NxParameterized::Traits* traits = mApexSDK->getParameterizedTraits();
			NxParameterized::Serializer* serializer = mApexSDK->createSerializer(NxParameterized::Serializer::NST_XML, traits);
			
			NxParameterized::Serializer::DeserializedData deserializedData;
			serializer->deserialize(*fileStream, deserializedData);
			if (1 != deserializedData.size())
			{
				printf("Error: requestResources found %i objects in %s\n", deserializedData.size(), filename.c_str());
				return NULL;
			}

			NxApexAsset* asset = mApexSDK->createAsset(deserializedData[0], name);
			if (!asset)
			{
				printf("Error: requestResources failed to create asset from %s\n", filename.c_str());
				return NULL;
			}

			return asset;
		}
		else
		{
			printf("requestResource: <%s> %s\n", nameSpace, name);
		}

		return NULL;
	}

	void releaseResource(const char* nameSpace, const char* name, void* resource)
	{
		ConsoleTextColor consoleColor(FOREGROUND_GREEN);
		if(!mApexSDK)
		{
			return;
		}
		
		printf("releaseResources called for <%s> %s\n", nameSpace, name);

		if (!strcmp(nameSpace, NX_APEX_EMITTER_AUTHORING_TYPE_NAME) ||
			!strcmp(nameSpace, NX_IOFX_AUTHORING_TYPE_NAME) ||
			!strcmp(nameSpace, NX_BASIC_IOS_AUTHORING_TYPE_NAME) ||
			!strcmp(nameSpace, NX_TURBULENCE_FS_AUTHORING_TYPE_NAME))
		{
			NxApexAsset* asset = reinterpret_cast<NxApexAsset*>(resource);
			asset->release();
		}
	}

	NxApexSDK*	mApexSDK;
	char		mPathToMedia[MAX_PATH];
	DummyMaterial material;
};

// A callback sprite buffer class for APEX rendering
class AppSpriteBuffer : public NxUserRenderSpriteBuffer
{
public:
	AppSpriteBuffer() : mContextData(NULL)
	{}

	void writeBuffer(const void* data, physx::PxU32 firstSprite, physx::PxU32 numSprites)
	{
		ConsoleTextColor consoleColor(FOREGROUND_RED);
		printf("writeBuffer called for %i sprites with ", numSprites - firstSprite);
		
		if (mContextData)
		{
			printf("this context: (%s)\n", mContextData);
		}
		else
		{
			printf("no context\n");
		}
		
		/* print position from data */
		if (firstSprite >= MAX_SPRITE_COUNT)
		{
			printf("Warning, writeBuffer called with firstSprite = %d", firstSprite);
			return;
		}

		if ((firstSprite + numSprites) > MAX_SPRITE_COUNT)
		{
			printf("Warning, writeBuffer called with %d sprites\n", numSprites);
			numSprites = MAX_SPRITE_COUNT - firstSprite;
		}

		SpriteData* spriteBuffer = &mSpriteData[firstSprite];
		memcpy(spriteBuffer, data, numSprites*sizeof(SpriteData));
		printf("Position Data: \n");
		for (physx::PxU32 i = 0; i < numSprites; i++)
		{
			const PxVec3& pos = spriteBuffer[i].position;
			printf(" (%.1f, %.1f, %.1f)\n", pos.x, pos.y, pos.z);
		}
	}

	struct SpriteData
	{
		PxVec3	position;
		PxF32	lifeRemaining;
	};

	static const PxU32	MAX_SPRITE_COUNT = 20;
	SpriteData			mSpriteData[MAX_SPRITE_COUNT];
	const char*			mContextData;
};

// A render resource callback class for APEX rendering
class AppRenderResource : public NxUserRenderResource
{
public:
	AppRenderResource::AppRenderResource()
		: mSpriteBuffer(NULL)
	{}

	/** \brief Set vertex buffer range */
	virtual void setVertexBufferRange(physx::PxU32 firstVertex, physx::PxU32 numVerts) {}
	/** \brief Set index buffer range */
	virtual void setIndexBufferRange(physx::PxU32 firstIndex, physx::PxU32 numIndices) {}
	/** \brief Set bone buffer range */
	virtual void setBoneBufferRange(physx::PxU32 firstBone, physx::PxU32 numBones) {}
	/** \brief Set instance buffer range */
	virtual void setInstanceBufferRange(physx::PxU32 firstInstance, physx::PxU32 numInstances) {}
	/** \brief Set sprite buffer range */
	virtual void setSpriteBufferRange(physx::PxU32 firstSprite, physx::PxU32 numSprites) 
	{
		ConsoleTextColor consoleColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
		printf("setSpriteBufferRange: first(%i) count(%i)\n", firstSprite, numSprites);
	}

	/** \brief Set material */
	virtual void setMaterial(void* material) 
	{}

	/** \brief Get number of vertex buffers */
	virtual physx::PxU32				getNbVertexBuffers() const 
	{
		return 0;
	}
	/** \brief Get vertex buffer */
	virtual NxUserRenderVertexBuffer*	getVertexBuffer(physx::PxU32 index) const 
	{
		return NULL;
	}

	/** \brief Get index buffer */
	virtual NxUserRenderIndexBuffer*	getIndexBuffer() const
	{
		return NULL;
	}

	/** \brief Get bone buffer */
	virtual NxUserRenderBoneBuffer*		getBoneBuffer() const 
	{
		return NULL;
	}

	/** \brief Get instance buffer */
	virtual NxUserRenderInstanceBuffer*	getInstanceBuffer() const
	{
		return NULL;
	}

	/** \brief Get sprite buffer */
	virtual NxUserRenderSpriteBuffer*	getSpriteBuffer() const
	{
		return mSpriteBuffer;
	}

	// directly accessed by the Render Resource Manager
	NxUserRenderSpriteBuffer*	mSpriteBuffer;
};

// A render resource manager for APEX (APEX will create render buffers through this interface)
class AppRenderResourceManager : public NxUserRenderResourceManager
{
public:
	// We're not using vertex, index, bone, or instance buffers in this exercise
	virtual NxUserRenderVertexBuffer*   createVertexBuffer(const NxUserRenderVertexBufferDesc& desc)     
	{
		return NULL;
	}
	virtual void                        releaseVertexBuffer(NxUserRenderVertexBuffer& buffer)            
	{}

	virtual NxUserRenderIndexBuffer*    createIndexBuffer(const NxUserRenderIndexBufferDesc& desc)       
	{
		return NULL;
	}
	virtual void                        releaseIndexBuffer(NxUserRenderIndexBuffer& buffer)              
	{}

	virtual NxUserRenderBoneBuffer*     createBoneBuffer(const NxUserRenderBoneBufferDesc& desc)         
	{
		return NULL;
	}
	virtual void                        releaseBoneBuffer(NxUserRenderBoneBuffer& buffer)                
	{}

	virtual NxUserRenderInstanceBuffer* createInstanceBuffer(const NxUserRenderInstanceBufferDesc& desc) 
	{
		return NULL;
	}
	virtual void                        releaseInstanceBuffer(NxUserRenderInstanceBuffer& buffer)        
	{}

	virtual NxUserRenderSurfaceBuffer* createSurfaceBuffer(const NxUserRenderSurfaceBufferDesc& desc) 
	{
		return NULL;
	}
	virtual void                        releaseSurfaceBuffer(NxUserRenderSurfaceBuffer& buffer)        
	{}

	// Just a predicate for removing the released buffers and resources from our stl lists
	class AppMatchesRenderResource
	{
	public:
		AppMatchesRenderResource::AppMatchesRenderResource(NxUserRenderResource* rr)
			: mToBeRemovedResource(rr)
		{}

		bool operator() (const AppRenderResource& value)
		{
			const NxUserRenderResource* rr = reinterpret_cast<const NxUserRenderResource*>(&value);
			return (rr == mToBeRemovedResource);
		}

		AppMatchesRenderResource::AppMatchesRenderResource(NxUserRenderSpriteBuffer* rr)
			: mToBeRemovedSpriteBuffer(rr)
		{}

		bool operator() (const AppSpriteBuffer& value)
		{
			const NxUserRenderSpriteBuffer* rr = reinterpret_cast<const NxUserRenderSpriteBuffer*>(&value);
			return (rr == mToBeRemovedSpriteBuffer);
		}

		NxUserRenderResource*		mToBeRemovedResource;
		NxUserRenderSpriteBuffer*	mToBeRemovedSpriteBuffer;
	};


	virtual NxUserRenderSpriteBuffer*   createSpriteBuffer(const NxUserRenderSpriteBufferDesc& desc)     
	{
		ConsoleTextColor consoleColor(FOREGROUND_BLUE|FOREGROUND_RED);
		printf("NxUserRenderResourceManager::createSpriteBuffer called\n");
		mSpriteBufferList.push_back(AppSpriteBuffer());
		return &(mSpriteBufferList.back());
	}

	virtual void                        releaseSpriteBuffer(NxUserRenderSpriteBuffer& buffer)            
	{
		ConsoleTextColor consoleColor(FOREGROUND_BLUE|FOREGROUND_RED);
		printf("NxUserRenderResourceManager::releaseSpriteBuffer called\n");
		mSpriteBufferList.remove_if(AppMatchesRenderResource(&buffer));
	}

	virtual NxUserRenderResource*       createResource(const NxUserRenderResourceDesc& desc)             
	{
		ConsoleTextColor consoleColor(FOREGROUND_GREEN|FOREGROUND_RED);
		printf("NxUserRenderResourceManager::createResource called\n");
		mRenderResourceList.push_back(AppRenderResource());
		mRenderResourceList.back().mSpriteBuffer = desc.spriteBuffer;
		
		// Let's setup the context so the sprite buffer's 'writeBuffer' method will know who it is
		AppSpriteBuffer* spriteBuffer = reinterpret_cast<AppSpriteBuffer*>(desc.spriteBuffer);
		spriteBuffer->mContextData = static_cast<const char*>(desc.userRenderData);
		
		return &(mRenderResourceList.back());
	}

	virtual void                        releaseResource(NxUserRenderResource& resource)                  
	{
		ConsoleTextColor consoleColor(FOREGROUND_GREEN|FOREGROUND_RED);
		printf("NxUserRenderResourceManager::releaseResource called\n");
		mRenderResourceList.remove_if(AppMatchesRenderResource(&resource));
	}

	virtual physx::PxU32                getMaxBonesForMaterial(void* material) 
	{
		return PX_MAX_U32;
	}

	/** \brief Get the sprite layout data */
	virtual bool getSpriteLayoutData(physx::PxU32 spriteCount, physx::PxU32 /*spriteSemanticsBitmap*/, physx::apex::NxUserRenderSpriteBufferDesc* bufferDesc)
	{
		bufferDesc->semanticOffsets[NxRenderSpriteLayoutElement::POSITION_FLOAT3] = offsetof(AppSpriteBuffer::SpriteData, position);
		bufferDesc->semanticOffsets[NxRenderSpriteLayoutElement::LIFE_REMAIN_FLOAT1] =  offsetof(AppSpriteBuffer::SpriteData, lifeRemaining);
		bufferDesc->stride = sizeof(AppSpriteBuffer::SpriteData);
		bufferDesc->maxSprites = min(spriteCount, AppSpriteBuffer::MAX_SPRITE_COUNT);
		bufferDesc->registerInCUDA = false;
		bufferDesc->textureCount = 0;
		return true;
	}

	/** \brief Get the instance layout data */
	virtual bool getInstanceLayoutData(physx::PxU32 /*spriteCount*/, physx::PxU32 /*spriteSemanticsBitmap*/, physx::apex::NxUserRenderInstanceBufferDesc* /*instanceDescArray*/)
	{
		PX_ALWAYS_ASSERT(); // TODO TODO TODO : This needs to be implemented.
		return false;
	}


	// These lists aren't required, but I thought it would be nice at some point
	// to know what resources are out there...
	std::list<AppRenderResource>	mRenderResourceList;
	std::list<AppSpriteBuffer>		mSpriteBufferList;
};


// A callback class for APEX that handles the "dispatch render resources" calls
class AppRenderer : public NxUserRenderer
{
	virtual void renderResource(const NxApexRenderContext& context)
	{
		//printf("renderResource called\n");
	}
};


// This class contains all of the different pointers and stuff for the program
// so we don't make a bunch of globals
class AppContext
{
public:
	AppContext::AppContext()
		: mFoundationSDK(NULL)
		, mPhysxSDK(NULL)
		, mPhysxCooking(NULL)
		, mPhysxScene(NULL)
		, mThreadPool(NULL)
		, mCudaContext(NULL)
		, mApexSDK(NULL)
		, mApexScene(NULL)
		, mParticlesModule(NULL)
		, mTurbulenceFSModule(NULL)
		, mIofxModule(NULL)
		, mLegacyModule(NULL)
		, mRenderVolume(NULL)
		, mEmitterAsset(NULL)
		, mEmitterActor(NULL)
		, mTurbulenceAsset(NULL)
		, mTurbulenceActor(NULL)
	{}

	bool initPhysX()
	{
		// Create the PhysX foundation
		mFoundationSDK = PxCreateFoundation(PX_PHYSICS_VERSION, mAppAllocator, mAppErrorCallback);
		if (!mFoundationSDK)
		{
			printf("Error initializing the foundation\n");
			return false;
		}

		// Create the PhysX SDK
		mPhysxSDK = PxCreatePhysics(PX_PHYSICS_VERSION, *mFoundationSDK, PxTolerancesScale());
		if (!mPhysxSDK)
		{
			printf("Error initializing PhysXSDK\n");
			return false;
		}

		mPhysxCooking = PxCreateCooking(PX_PHYSICS_VERSION, mPhysxSDK->getFoundation(), PxCookingParams(mPhysxSDK->getTolerancesScale()));
		if (!mPhysxCooking)
		{
			printf("Error initializing PhysXSDK Cooking\n");
			return false;
		}

		// Create the PhysX SDK CPU Thread Pool
		mThreadPool = PxDefaultCpuDispatcherCreate(4);

		// Create the CUDA context manager (APEX will use this as well, it retrieves it from PhysX)
		physx::PxCudaContextManagerDesc ctxMgrDesc;
		// this call simply returns NULL on platforms and configurations that don't support CUDA
		mCudaContext = PxCreateCudaContextManager(mPhysxSDK->getFoundation(), ctxMgrDesc, mPhysxSDK->getProfileZoneManager());
		if (mCudaContext && !mCudaContext->contextIsValid())
		{
			mCudaContext->release();
			mCudaContext = NULL;
			printf("Error creating CUDA Context Manager\n");
			return false;
		}

		// Create the PhysX SDK scene
		PxSceneDesc desc(mPhysxSDK->getTolerancesScale());
		desc.cpuDispatcher = mThreadPool;
		desc.gpuDispatcher = mCudaContext->getGpuDispatcher();
		desc.filterShader = PxDefaultSimulationFilterShader;
		if (!desc.isValid())
		{
			printf("Error, invalid PhysX scene descriptor\n");
			return false;
		}

		mPhysxScene = mPhysxSDK->createScene(desc);
		if (!mPhysxScene)
		{
			printf("Error initializing PhysX scene\n");
			return false;
		}

		return true;
	}

	void destroyPhysX()
	{
		releaseAndClear(mPhysxScene);
		releaseAndClear(mCudaContext);
		releaseAndClear(mThreadPool);
		releaseAndClear(mPhysxCooking);
		releaseAndClear(mPhysxSDK);
		releaseAndClear(mFoundationSDK);
	}

	bool initAPEX()
	{
		// Create the APEX SDK
		NxApexSDKDesc apexDesc;
		apexDesc.physXSDK              = mPhysxSDK;
		apexDesc.cooking               = mPhysxCooking;
		apexDesc.renderResourceManager = &mApexRenderResourceManager;
		apexDesc.resourceCallback      = &mApexResourceCallback;
		apexDesc.wireframeMaterial	   = "materials/simple_unlit.xml";
		apexDesc.solidShadedMaterial   = "materials/simple_lit_color.xml";
		NxApexCreateError errorCode;
		mApexSDK = NxCreateApexSDK(apexDesc, &errorCode);
		if (!mApexSDK)
		{
			printf("Error creating APEX SDK: %i\n", errorCode);
			return false;
		}

		mApexResourceCallback.setApexSDK(mApexSDK);

		// Load the necessary particle modules
		mParticlesModule = static_cast< NxModuleParticles *>(mApexSDK->createModule("Particles"));
		if (mParticlesModule)
		{
			mIofxModule = static_cast<physx::apex::NxModuleIofx*>(mParticlesModule->getModule("IOFX"));
		}
		mTurbulenceFSModule = mApexSDK->createModule("TurbulenceFS");

		// Load the legacy modules (in case someone upgrades our asset classes in APEX)
		mLegacyModule = mApexSDK->createModule("Legacy");
		
		if (!mParticlesModule ||
			!mIofxModule)
		{
			printf("Error loading particle modules\n");
			return false;
		}

		// Create the APEX scene
		NxApexSceneDesc apexSceneDesc;
		apexSceneDesc.scene = mPhysxScene;
		apexSceneDesc.debugVisualizeLocally = false;
		apexSceneDesc.debugVisualizeRemotely = false;
		if (!apexSceneDesc.isValid())
		{
			printf("Invalid APEX Scene Desc\n");
			return false;
		}

		mApexScene = mApexSDK->createScene(apexSceneDesc);		
		if (!mApexScene)
		{
			printf("Error creating APEX Scene\n");
			return false;
		}

		// Allocate the view and projection matrices
		mApexScene->allocViewMatrix(ViewMatrixType::LOOK_AT_RH);
		mApexScene->allocProjMatrix(ProjMatrixType::USER_CUSTOMIZED);

		// We don't want LOD messing with us at the moment
		mApexScene->setLODResourceBudget(PX_MAX_F32);

		// Create a render volume for the particles
		PxBounds3 infBounds;
		infBounds.setMaximal();
		mRenderVolume = mIofxModule->createRenderVolume(*mApexScene, infBounds, 0, true);
		return true;
	}

	void destroyAPEX()
	{
		if (mRenderVolume)
		{
			mIofxModule->releaseRenderVolume(*mRenderVolume);
			mRenderVolume = NULL;
		}
			
		releaseAndClear(mApexScene);
		releaseAndClear(mParticlesModule);
		mIofxModule = NULL;
		releaseAndClear(mTurbulenceFSModule);
		releaseAndClear(mLegacyModule);

		mApexResourceCallback.setApexSDK(NULL);
		releaseAndClear(mApexSDK);
	}

	// This method creates the emitter asset and actor
	bool initAssetsAndActors(bool useTurbulence)
	{
		if (!mApexScene)
		{
			return false;
		}

		// Load the emitter asset and create an actor
		NxResourceProvider* NRP = mApexSDK->getNamedResourceProvider();
		
		// emitter asset and actor
		{
			// This explicit emitter contains no particles in the asset, it is intended
			// to simply allow the app to create particles explicitely
			const char* emitterAssetName = "explicitEmitterAsset";
			mEmitterAsset = reinterpret_cast<NxApexAsset*>(NRP->getResource(NX_APEX_EMITTER_AUTHORING_TYPE_NAME, emitterAssetName));
			if (mEmitterAsset)
			{
				// bump your refcount so that things work smoothly
				NRP->setResource(NX_APEX_EMITTER_AUTHORING_TYPE_NAME, emitterAssetName, mEmitterAsset, true);
			}
			else
			{
				printf("Failed to create the APEX Emitter Asset\n");
				return false;
			}
		
			// get the actor creation parameters from the asset
			NxParameterized::Interface* actorParams = mEmitterAsset->getDefaultActorDesc();
		
			// this will prevent the particles added to the actor from being emitted by the asset's particle list
			// this is a little confusing, but you'll get double the particles if you don't do this
			// these parameters can be found in the NxParameterized documentation
			NxParameterized::setParamBool(*actorParams, "emitAssetParticles", false);
		
			mEmitterActor = mEmitterAsset->createApexActor(*actorParams, *mApexScene);
			if (!mEmitterActor)
			{
				printf("Failed to create the APEX Emitter Actor\n");
				return false;
			}

			// tell the emitter to emit the particles it finds in its list every frame
			// We will be using the emitter in a mode where we clear the insertion particle list every frame
			// and add one of our own.
			NxApexEmitterActor* actor = reinterpret_cast<NxApexEmitterActor*>(mEmitterActor);
			actor->startEmit(true);
		}

		// turbulence asset and actor
		if (useTurbulence)
		{
			const char* turbulenceAssetName = "turbulenceFSAsset";
			mTurbulenceAsset = reinterpret_cast<NxApexAsset*>(NRP->getResource(NX_TURBULENCE_FS_AUTHORING_TYPE_NAME, turbulenceAssetName));
			if (mTurbulenceAsset)
			{
				// bump your refcount so that things work smoothly
				NRP->setResource(NX_TURBULENCE_FS_AUTHORING_TYPE_NAME, turbulenceAssetName, mTurbulenceAsset, true);
			}
			else
			{
				printf("Failed to create the Turbulence Asset\n");
				return false;
			}

			// get the actor creation parameters from the asset
			NxParameterized::Interface* actorParams = mTurbulenceAsset->getDefaultActorDesc();
			mTurbulenceActor = mTurbulenceAsset->createApexActor(*actorParams, *mApexScene);
			if (!mTurbulenceActor)
			{
				printf("Failed to create the Turbulence Actor\n");
				return false;
			}
			NxTurbulenceFSActor* actor = reinterpret_cast<NxTurbulenceFSActor*>(mTurbulenceActor);
			actor->setEnabled(true);
			
			// the grid is placed with the bottom just 1 unit above the origin, this way
			// the particles move up freely for one frame, then begin to slow once they are in the grid
			PxVec3 gridSize = actor->getGridSize();
			PxMat44 pose = PxMat44::createIdentity();
			pose.setPosition(PxVec3(0.0f, gridSize.y * 0.5f + 1.0f, 0.0f));
			actor->setPose(pose);

			// an external acceleration gives us a more interesting setup
			actor->setExternalVelocity(PxVec3(60.0f, 0.0f, 0.0f));
		}

		return true;
	}

	void destroyAssetsAndActors()
	{
		releaseAndClear(mEmitterActor);
		// instead of releasing the asset directly, allow the NRP to do it because we used the
		// NRP to create it
		if (mEmitterAsset)
		{
			NxResourceProvider* NRP = mApexSDK->getNamedResourceProvider();
			NRP->releaseResource(mEmitterAsset->getObjTypeName(), mEmitterAsset->getName());
			mEmitterAsset = NULL;
		}

		releaseAndClear(mTurbulenceActor);
		// instead of releasing the asset directly, allow the NRP to do it because we used the
		// NRP to create it
		if (mTurbulenceAsset)
		{
			NxResourceProvider* NRP = mApexSDK->getNamedResourceProvider();
			NRP->releaseResource(mTurbulenceAsset->getObjTypeName(), mTurbulenceAsset->getName());
			mTurbulenceAsset = NULL;
		}

	}

	// this method just adds a single particle at the origin, shooting straight up (y-up)
	void addParticle()
	{
		if (!mEmitterActor)
		{
			printf("Emitter actor is not initialized\n");
			return;
		}

		NxApexEmitterActor* actor = reinterpret_cast<NxApexEmitterActor*>(mEmitterActor);
		NxEmitterExplicitGeom* geom = actor->isExplicitGeom();
		if (geom)
		{
			PxVec3 pos(0.0f);
			PxVec3 vel(0.0f, 60.0f, 0.0f);
			
			geom->resetParticleList();
			geom->addParticleList(1, &pos, &vel);
		}
	}

	// this method calls the render API on the render volume's IOFX actors
	// our callbacks just print the particle positions
	void printParticleData()
	{
		physx::PxU32 numActors;
		physx::PxU32 drawnParticles = 0;

		mRenderVolume->lockRenderResources();
		NxIofxActor* const* actors = mRenderVolume->getIofxActorList(numActors);
		for (physx::PxU32 j = 0 ; j < numActors ; j++)
		{
			actors[j]->lockRenderResources();
			if (!actors[j]->getBounds().isEmpty())
			{
				// more explaination of this context required
				static char* emitterParticleContext = "EmitterParticleDataContext";
				actors[j]->updateRenderResources(false, emitterParticleContext);
				// for our purposes (printing the positions to STDOUT), DRR is not required
				//actors[j]->dispatchRenderResources(mApexRenderer);
				drawnParticles += actors[j]->getObjectCount();
			}
			actors[j]->unlockRenderResources();
		}
		mRenderVolume->unlockRenderResources();
	}

	// dirt simple simulate-fetchresults, blocking during simulation
	void simulateFrame(PxF32 dt)
	{
		if (!mApexScene)
		{
			printf("Error, no APEX Scene created\n");
			return;
		}
		else
		{
			mApexScene->simulate(dt);
			PxU32 errorState = 0;
			mApexScene->fetchResults(true, &errorState);
			if (errorState)
			{
				printf("Error simulating APEX: %i\n", errorState);
			}

			mApexScene->prepareRenderResourceContexts();
		}
	}

	// Callback classes
	AppAlloc					mAppAllocator;
	AppErrorCallback			mAppErrorCallback;
	AppApexResourceCallback		mApexResourceCallback;
	AppRenderResourceManager	mApexRenderResourceManager;
	AppRenderer					mApexRenderer;
	
	// PhysX pointers
	PxFoundation*				mFoundationSDK;
	PxPhysics*					mPhysxSDK;
	PxCooking*					mPhysxCooking;
	PxScene*					mPhysxScene;
	PxDefaultCpuDispatcher*		mThreadPool;
	PxCudaContextManager*		mCudaContext;

	// APEX pointers
	NxApexSDK*					mApexSDK;
	NxApexScene*				mApexScene;
	NxModuleParticles*			mParticlesModule;
	NxModule*					mTurbulenceFSModule;
	NxModuleIofx*				mIofxModule;
	NxModule*					mLegacyModule;
	NxApexRenderVolume*			mRenderVolume;
	NxApexAsset*				mEmitterAsset;
	NxApexActor*				mEmitterActor;
	NxApexAsset*				mTurbulenceAsset;
	NxApexActor*				mTurbulenceActor;
};


// command line arg "noTurbulence" will simulate without the turbulence actor
int main(int argc, char **argv)
{
	printf("APEX Particle Sample\n");

	bool useTurbulence = true;
	if (argc>1)
	{
		if (!stricmp(argv[1], "noTurbulence"))
		{
			useTurbulence = false;
		}
	}

	AppContext app;
	if (!app.initPhysX())
	{
		printf("PhysX initialization failed, exiting\n");
		return 1;
	}
	
	if (!app.initAPEX())
	{
		printf("APEX initialization failed, exiting\n");
		return 1;
	}

	if (!app.initAssetsAndActors(useTurbulence))
	{
		printf("Asset and Actor initialization failed, exiting\n");
		return 1;
	}

	// Simulate 8 frames, add a particle before each frame
	for(PxU32 i=0; i<8; i++)
	{
		app.addParticle();
		app.simulateFrame(1.0f/60.0f);
		app.printParticleData();
	}

	app.destroyAssetsAndActors();
	app.destroyAPEX();
	app.destroyPhysX();	

	return 0;
}

// This project, for no good reason, needs to be a windows executable project, so we'll just wrap the cmd line 
// arg and pass to main
int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE, LPSTR cmdLine, int show_command)
{
	//SampleFramework::SampleCommandLine cl(GetCommandLineA());
	//bool ok = SampleEntry(cl);
	//return ok ? 0 : 1;

	if (AllocConsole())
	{
		FILE* stream;
		freopen_s(&stream, "CONOUT$", "wb", stdout);
		freopen_s(&stream, "CONOUT$", "wb", stderr);
		freopen_s(&stream, "CONIN$", "r", stdin);

		SetConsoleTitle("MinimalTurbulence Console");
		//SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED);

		CONSOLE_SCREEN_BUFFER_INFO coninfo;
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
		coninfo.dwSize.Y = 1000;
		SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);
	}

	// A very poor man's argv/argc
	int argc = 2;
	char* argv[2] = {"MinimalTurbulence", cmdLine};

	main(argc, argv);

	printf("Press ENTER to exit\n");
	getc(stdin);

	return 0;
}

#endif //PX_WINDOWS
