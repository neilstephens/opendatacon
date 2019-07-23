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
		set(TOP_INCLUDE ${CMAKE_FIND_ROOT_PATH}/${PYTHON_HOME}/include)
	else()
		set(TOP_INCLUDE ${PYTHON_HOME}/include)
		file(GLOB_RECURSE PYTHON_H ${PYTHON_HOME}/*Python.h)
	endif()
	message("Globbed ${PYTHON_H}")
	string(REGEX MATCH "([^;]*)/include/(python3.([0-9])+m)" PYTHON_VER "${PYTHON_H}")
	set(PYTHON_HOME_DISCOVERED ${CMAKE_MATCH_1})
	set(PYTHON_VER ${CMAKE_MATCH_2})
	set(PYTHON_MINOR_VER ${CMAKE_MATCH_3})
	message("Version string: ${PYTHON_VER}")
	if(${PYTHON_HOME_DISCOVERED} EQUAL ${PYTHON_HOME})
		message("PYTHON_HOME confirmed")
	else()
		message("Warning: resetting PYTHON_HOME to ${PYTHON_HOME_DISCOVERED}")
		set(PYTHON_HOME ${PYTHON_HOME_DISCOVERED})
	endif()

	find_path(PYTHON_INCLUDE_DIRS Python.h
		PATHS ${PYTHON_HOME}/include/${PYTHON_VER}
		NO_DEFAULT_PATH
		CMAKE_FIND_ROOT_PATH_BOTH)

	if(PYTHON_INCLUDE_DIRS)
		message("Python headers found: ${PYTHON_INCLUDE_DIRS}")
	else()
		message(FATAL_ERROR "Can't find Python headers in ${PYTHON_HOME}" )
	endif()

	set(PYTHON_INCLUDE_DIRS ${PYTHON_INCLUDE_DIRS} ${TOP_INCLUDE})

	string(REGEX MATCH "3[0-9]+" PYTHON_NUM "${PYTHON_INCLUDE_DIRS}")

	if((${PYTHON_NUM} LESS 35) OR (${PYTHON_MINOR_VER} LESS 5))
		add_definitions(-DPYTHON_34_ORLESS)
	endif()

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
