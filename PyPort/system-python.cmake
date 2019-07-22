if(NOT USE_PYTHON_SUBMODULE)

	if(DEFINED PYTHON_HOME)
		set(PYTHON_HOME ${PYTHON_HOME} CACHE PATH ${PYTHON_HOME_INSTRUCTIONS})
	else()
		set(PYTHON_HOME "/usr" CACHE PATH ${PYTHON_HOME_INSTRUCTIONS})
	endif()

	#find python headers
	file(GLOB_RECURSE PYTHON_H ${PYTHON_HOME}/*Python.h)
	string(REGEX MATCH "python3.[0-9]+m" PYTHON_VER "${PYTHON_H}")
	find_path(PYTHON_INCLUDE_DIRS Python.h
		PATHS ${PYTHON_HOME}/include/${PYTHON_VER} ${PYTHON_HOME}/local/include/${PYTHON_VER}
		NO_DEFAULT_PATH
		CMAKE_FIND_ROOT_PATH_BOTH)

	if(PYTHON_INCLUDE_DIRS)
		message("Python.h found: ${PYTHON_INCLUDE_DIRS}")
	else()
		message(FATAL_ERROR "Can't find Python.h in ${PYTHON_HOME}" )
	endif()

	#import the python lib
	find_library(PYTHON_LIB NAMES ${PYTHON_VER} lib${PYTHON_VER}
		PATHS ${PYTHON_HOME}/lib ${PYTHON_HOME}/local/lib
		PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE}
		NO_DEFAULT_PATH
		CMAKE_FIND_ROOT_PATH_BOTH)

	add_library(python_target UNKNOWN IMPORTED)
	set_property(TARGET python_target PROPERTY IMPORTED_LOCATION "${PYTHON_LIB}")
	#set a variable to use for linking
	set(PYTHON_LIBRARIES debug python_target optimized python_target )

	set(PYTHON_LIBRARY_DEBUG "${PYTHON_LIB}")
	set(PYTHON_LIBRARY_RELEASE "${PYTHON_LIB}")

endif()
