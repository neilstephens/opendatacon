set( SNAPPY_INCLUDE_DIR "" CACHE PATH "The path to the Snappy include files")

find_path(
  SNAPPY_INCLUDE_DIR
  NAMES snappy.h
  HINTS ${SNAPPY_ROOT_DIR}/include)

find_library(
  SNAPPY_LIBRARY
  NAMES snappy 
  HINTS ${SNAPPY_ROOT_DIR}/lib)
  
 find_library(
  SNAPPY_DEBUG_LIBRARY
  NAMES snappyd 
  HINTS ${SNAPPY_ROOT_DIR}/lib) 

#TODO Find dynamic libraries if specified
#if (BUILD_SHARED_LIBS)

set( SNAPPY_LIBRARIES "" CACHE PATH "The list of Snappy Libraries")
if(UNIX)
	if (NOT (${SNAPPY_LIBRARY} STREQUAL "SNAPPY_LIBRARY-NOTFOUND"))
		list( APPEND SNAPPY_LIBRARIES "${SNAPPY_LIBRARY}")
	endif()
else()
	#Handle the case where we are only building for only optimized or debug release. 
	if (NOT (${SNAPPY_LIBRARY} STREQUAL "SNAPPY_LIBRARY-NOTFOUND"))
		list( APPEND SNAPPY_LIBRARIES optimized "${SNAPPY_LIBRARY}")
	endif()

	if (NOT (${SNAPPY_DEBUG_LIBRARY} STREQUAL "SNAPPY_DEBUG_LIBRARY-NOTFOUND"))
		list( APPEND SNAPPY_LIBRARIES debug "${SNAPPY_DEBUG_LIBRARY}")
	endif()
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
  Snappy DEFAULT_MSG
  SNAPPY_LIBRARIES
  SNAPPY_INCLUDE_DIR)

# This should make them only visible when advanced is ticked
mark_as_advanced(
  SNAPPY_ROOT_DIR
  SNAPPY_LIBRARIES
  SNAPPY_INCLUDE_DIR)
