if(NOT USE_PYTHON_SUBMODULE)

	if(DEFINED PYTHON_HOME)
		set(PYTHON_HOME ${PYTHON_HOME} CACHE PATH ${PYTHON_HOME_INSTRUCTIONS})
	else()
		set(PYTHON_HOME "/usr" CACHE PATH ${PYTHON_HOME_INSTRUCTIONS})
	endif()

	#find python headers
	file(GLOB_RECURSE PYTHON_H ${CMAKE_FIND_ROOT_PATH}/${PYTHON_HOME}/*Python.h)
	if(PYTHON_H)
		message("Found Python.h using CMAKE_FIND_ROOT_PATH")
	else()
		file(GLOB_RECURSE PYTHON_H ${PYTHON_HOME}/*Python.h)
	endif()
	message("Globbed ${PYTHON_H}")
	string(REGEX MATCH "python3.[0-9]+m" PYTHON_VER "${PYTHON_H}")
	message("Version string: ${PYTHON_VER}")
	find_path(PYTHON_INCLUDE_DIRS Python.h
		PATHS ${PYTHON_HOME}/include/${PYTHON_VER} ${PYTHON_HOME}/local/include/${PYTHON_VER}
		NO_DEFAULT_PATH
		CMAKE_FIND_ROOT_PATH_BOTH)

	if(PYTHON_INCLUDE_DIRS)
		message("Python.h found: ${PYTHON_INCLUDE_DIRS}")
	else()
		message("Can't find Python.h in ${PYTHON_HOME}" )
	endif()

	string(REGEX MATCH "3[0-9]+" PYTHON_NUM "${PYTHON_INCLUDE_DIRS}")

	#import the python lib
	find_library(PYTHON_LIB NAMES ${PYTHON_VER} lib${PYTHON_VER} libpython${PYTHON_NUM} python${PYTHON_NUM}
		PATHS ${PYTHON_HOME}/lib ${PYTHON_HOME}/libs ${PYTHON_HOME}/local/lib ${PYTHON_HOME}/lib64 ${PYTHON_HOME}/local/lib64
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
