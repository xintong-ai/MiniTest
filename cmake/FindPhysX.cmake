if(NOT PHYSX_SDK_PATH)
    find_path(PHYSX_SDK_PATH foundation/include/Px.h)
elseif(PHYSX_SDK_PATH AND PHYSX_LOADER_LIBRARY)
    set(PHYSX_FIND_QUIETLY TRUE)
endif()


#This isn't quite right, but we'll assume they are all at the same place.
find_path(PHYSX_SDK_PATH Include/foundation/Px.h
          /usr/local/include
          /usr/include
          "C:/Program Files/AGEIA Technologies/AGEIA PhysX SDK/v2.6.4/SDKs"
          "C:/Program Files (x86)/AGEIA Technologies/AGEIA PhysX SDK/v2.6.4/SDKs"
)

if(PHYSX_SDK_PATH)
    set(PHYSX_FOUNDATION_INCLUDE_DIR "${PHYSX_SDK_PATH}/Include/foundation")
    set(PHYSX_COOKING_INCLUDE_DIR "${PHYSX_SDK_PATH}/Include/cooking")
    set(PHYSX_FRAMEWORK_INCLUDE_DIR "${PHYSX_SDK_PATH}/Include/framework")
    set(PHYSX_CHARACTER_INCLUDE_DIR "${PHYSX_SDK_PATH}/Include/characterkinematic/include")
    set(PHYSX_EXTENSIONS_INCLUDE_DIR "${PHYSX_SDK_PATH}/Include/extensions")
    #set(PHYSX_PHYSICS_INCLUDE_DIR "${PHYSX_SDK_PATH}/Physics/include")
    #set(PHYSX_LOADER_INCLUDE_DIR "${PHYSX_SDK_PATH}/PhysXLoader/include")

    set(PHYSX_INCLUDE_DIRS
		${PHYSX_SDK_PATH}/Include
        ${PHYSX_COOKING_INCLUDE_DIR}
        ${PHYSX_FOUNDATION_INCLUDE_DIR}
        ${PHYSX_FRAMEWORK_INCLUDE_DIR}
        ${PHYSX_CHARACTER_INCLUDE_DIR}
        ${PHYSX_EXTENSIONS_INCLUDE_DIR}
#        ${PHYSX_PHYSICS_INCLUDE_DIR}
#        ${PHYSX_LOADER_INCLUDE_DIR}
    )
endif()

#2.7.3 is the earliest version that can be downloaded from nVidia's archive.
find_library(PHYSX_COOKING_LIBRARY NAMES PhysX3Cooking_x64
             PATH_SUFFIXES "PhysX/v2.7.3"
             PATHS "${PHYSX_SDK_PATH}/Lib/vc12win64"
)
find_library(PHYSX_CHARACTER_LIBRARY NAMES PhysX3CharacterKinematic_x64
             PATH_SUFFIXES "PhysX/v2.7.3"
             PATHS "${PHYSX_SDK_PATH}/Lib/vc12win64"
)
# find_library(PHYSX_EXTENSIONS_LIBRARY NAMES NxExtensions
             # PATH_SUFFIXES "PhysX/v2.7.3"
             # PATHS "${PHYSX_SDK_PATH}/Lib/vc12win64"
# )
# find_library(PHYSX_LOADER_LIBRARY NAMES PhysXLoader
             # PATHS "${PHYSX_SDK_PATH}/Lib/vc12win64"
# )

set(PHYSX_LIBRARIES
    ${PHYSX_COOKING_LIBRARY}
    ${PHYSX_CHARACTER_LIBRARY}
    # ${PHYSX_EXTENSIONS_LIBRARY}
    # ${PHYSX_LOADER_LIBRARY}
)

mark_as_advanced(PHYSX_COOKING_LIBRARY
                 PHYSX_CHARACTER_LIBRARY
                 # PHYSX_EXTENSIONS_LIBRARY
                 # PHYSX_LOADER_LIBRARY
				 )

if(PHYSX_SDK_PATH AND PHYSX_LOADER_LIBRARY)
    set(PHYSX_FOUND TRUE)
endif()

if (PHYSX_FOUND)
    if(NOT PHYSX_FIND_QUIETLY)
        message(STATUS "Found PhysX: ${PHYSX_SDK_PATH}")
    endif()
else()
    if(PHYSX_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find PhysX.  Please set your PHYSX_SDK_PATH appropriately and try again")
    endif()
endif()
