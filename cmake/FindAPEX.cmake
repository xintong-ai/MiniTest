# - Check for the presence of APEX
#
# The following variables are set when APEX is found:
#  HAVE_APEX       = Set to true, if all components of APEX
#                          have been found.
#  APEX_INCLUDE_DIRS   = Include path for the header files of APEX
#  APEX_LIBRARIES  = Link these to use APEX

## -----------------------------------------------------------------------------
## Check for the header files

find_path (APEX_SDK_PATH framework/public/NxApex.h
  PATHS /usr/local/include /usr/include /sw/include
  PATH_SUFFIXES 
  )

## -----------------------------------------------------------------------------
## Check for the library

if(APEX_SDK_PATH)
	set(APEX_INCLUDE_DIRS 
	#	${APEX_SDK_PATH}/framework/include
		${APEX_SDK_PATH}/framework/public
		${APEX_SDK_PATH}/framework/public/PhysX3
		${APEX_SDK_PATH}/shared/general/PxIOStream/public
		${APEX_SDK_PATH}/NxParameterized/public
		${APEX_SDK_PATH}/module/emitter/public
		${APEX_SDK_PATH}/module/iofx/public
		${APEX_SDK_PATH}/module/basicios/public
		
	)
endif()



find_library (APEX_FRAMEWORK_LIBRARY ApexFramework_x64
  PATHS /usr/local/lib /usr/lib /lib /sw/lib
  PATH_SUFFIXES lib/vc12win64-PhysX_3.3
  )

find_library (APEX_CLOTHING_LIBRARY APEX_Clothing_x64
  PATHS /usr/local/lib /usr/lib /lib /sw/lib
  PATH_SUFFIXES lib/vc12win64-PhysX_3.3
  )

set(APEX_LIBRARIES 
	${APEX_FRAMEWORK_LIBRARY}
	${APEX_CLOTHING_LIBRARY}
  )

## -----------------------------------------------------------------------------
## Actions taken when all components have been found

if (APEX_INCLUDE_DIRS AND APEX_LIBRARIES)
  set (HAVE_APEX TRUE)
else (APEX_INCLUDE_DIRS AND APEX_LIBRARIES)
  if (NOT APEX_FIND_QUIETLY)
    if (NOT APEX_INCLUDE_DIRS)
      message (STATUS "Unable to find APEX header files!")
    endif (NOT APEX_INCLUDE_DIRS)
    if (NOT APEX_LIBRARIES)
      message (STATUS "Unable to find APEX library files!")
    endif (NOT APEX_LIBRARIES)
  endif (NOT APEX_FIND_QUIETLY)
endif (APEX_INCLUDE_DIRS AND APEX_LIBRARIES)

if (HAVE_APEX)
  if (NOT APEX_FIND_QUIETLY)
    message (STATUS "Found components for APEX")
    message (STATUS "APEX_INCLUDE_DIRS = ${APEX_INCLUDE_DIRS}")
    message (STATUS "APEX_LIBRARIES     = ${APEX_LIBRARIES}")
  endif (NOT APEX_FIND_QUIETLY)
else (HAVE_APEX)
  if (APEX_FIND_REQUIRED)
    message (FATAL_ERROR "Could not find APEX!")
  endif (APEX_FIND_REQUIRED)
endif (HAVE_APEX)

mark_as_advanced (
  HAVE_APEX
  APEX_LIBRARIES
  APEX_INCLUDE_DIRS
  )