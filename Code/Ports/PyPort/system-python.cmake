if(NOT USE_PYTHON_SUBMODULE)

	if(DEFINED PYTHON_HOME)
		set(PYTHON_HOME ${PYTHON_HOME} CACHE PATH ${PYTHON_HOME_INSTRUCTIONS})
	else()
		set(PYTHON_HOME "/usr" CACHE PATH ${PYTHON_HOME_INSTRUCTIONS})
	endif()

	#find python headers
	file(GLOB_RECURSE PYTHON_H ${CMAKE_FIND_ROOT_PATH}${PYTHON_HOME}/*Python.h)
	if(PYTHON_H)
		message("Found Python.h using CMAKE_FIND_ROOT_PATH")
		set(TOP_INCLUDE ${CMAKE_FIND_ROOT_PATH}${PYTHON_HOME}/include)
	else()
		set(TOP_INCLUDE ${PYTHON_HOME}/include)
		file(GLOB_RECURSE PYTHON_H ${PYTHON_HOME}/*Python.h)
	endif()
	message("Globbed '${PYTHON_H}'")
	string(REGEX MATCH "([^;]*)[/]+include[/]+(python3.([0-9])+)m?" PYTHON_VER "${PYTHON_H}")
	if(PYTHON_VER)
		set(PYTHON_HOME_DISCOVERED ${CMAKE_MATCH_1})
		set(PYTHON_VER ${CMAKE_MATCH_2})
		set(PYTHON_MINOR_VER ${CMAKE_MATCH_3})
		message("Version string: ${PYTHON_VER}")
		if("${PYTHON_HOME_DISCOVERED}" STREQUAL "${PYTHON_HOME}")
			message("PYTHON_HOME confirmed")
		else()
			message("Warning: resetting PYTHON_HOME (${PYTHON_HOME}) to (${PYTHON_HOME_DISCOVERED})")
			set(PYTHON_HOME ${PYTHON_HOME_DISCOVERED})
		endif()
	endif()

	find_path(PYTHON_INCLUDE_DIRS Python.h
		PATHS ${PYTHON_HOME}/include/${PYTHON_VER}m ${PYTHON_HOME}/include/${PYTHON_VER} ${PYTHON_HOME}/include
		NO_DEFAULT_PATH
		CMAKE_FIND_ROOT_PATH_BOTH)

	if(PYTHON_INCLUDE_DIRS)
		message("Python headers found: ${PYTHON_INCLUDE_DIRS}")
	else()
		message(FATAL_ERROR "Can't find Python headers in ${PYTHON_HOME}" )
	endif()

	set(PYTHON_INCLUDE_DIRS ${PYTHON_INCLUDE_DIRS} ${TOP_INCLUDE})

	string(REGEX MATCH "(3)\.?([0-9]+)" PYTHON_NUM "${PYTHON_INCLUDE_DIRS}")
	if(PYTHON_NUM)
		set(PYTHON_NUM ${CMAKE_MATCH_1}${CMAKE_MATCH_2})
	endif()

	if(PYTHON_NUM OR PYTHON_MINOR_VER)
		if(PYTHON_NUM)
			message("Python version number ${PYTHON_NUM}")
		endif()
		if(PYTHON_MINOR_VER)
			message("Python version 3.${PYTHON_MINOR_VER}")
		endif()
		if((PYTHON_NUM LESS 35) OR (PYTHON_MINOR_VER LESS 5))
			add_definitions(-DPYTHON_34_ORLESS)
		endif()
	else()
		message("Warning: Python version unknown")
	endif()

	if(WIN32)
		set(PYTHON_DEBUG_POSTFIX "_d")
	endif()

	#import the python lib
	find_library(PYTHON_LIBRARY_RELEASE
		NAMES ${PYTHON_VER}m
			${PYTHON_VER}
			lib${PYTHON_VER}m
			lib${PYTHON_VER}
			libpython${PYTHON_NUM}
			python${PYTHON_NUM}
		PATHS ${PYTHON_HOME}/lib ${PYTHON_HOME}/libs ${PYTHON_HOME}/local/lib ${PYTHON_HOME}/lib64 ${PYTHON_HOME}/local/lib64
		PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE}
		NO_DEFAULT_PATH
		CMAKE_FIND_ROOT_PATH_BOTH)
	add_library(python_target UNKNOWN IMPORTED)
	set_property(TARGET python_target PROPERTY IMPORTED_LOCATION "${PYTHON_LIBRARY_RELEASE}")

	#import the debug python lib
	find_library(PYTHON_LIBRARY_DEBUG
		NAMES ${PYTHON_VER}m${PYTHON_DEBUG_POSTFIX}
			${PYTHON_VER}${PYTHON_DEBUG_POSTFIX}
			lib${PYTHON_VER}m${PYTHON_DEBUG_POSTFIX}
			lib${PYTHON_VER}${PYTHON_DEBUG_POSTFIX}
			libpython${PYTHON_NUM}${PYTHON_DEBUG_POSTFIX}
			python${PYTHON_NUM}${PYTHON_DEBUG_POSTFIX}
		PATHS ${PYTHON_HOME}/lib ${PYTHON_HOME}/libs ${PYTHON_HOME}/local/lib ${PYTHON_HOME}/lib64 ${PYTHON_HOME}/local/lib64
		PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE}
		NO_DEFAULT_PATH
		CMAKE_FIND_ROOT_PATH_BOTH)

	if(WIN32)
		if (NOT PYTHON_LIBRARY_DEBUG)
			message(FATAL_ERROR "Error: Can't find Python debug lib" )
		endif()
	endif()

	add_library(python_target_d UNKNOWN IMPORTED)
	set_property(TARGET python_target_d PROPERTY IMPORTED_LOCATION "${PYTHON_LIBRARY_DEBUG}")

	#set a variable to use for linking
	set(PYTHON_LIBRARIES debug python_target_d optimized python_target )

	#include in install/packaging
	option(PACKAGE_PYTHON "Package python libs in c-pack installer" ON)
	if(PACKAGE_PYTHON)
		set(PACK_NAMES python expat zlib tinfo sqlite readline ncurses mpdec lzma ffi db5 bz2)
		find_path(PYTHON_STDLIB_DIR _pydecimal.py
			PATHS ${PYTHON_HOME}/lib/${PYTHON_VER} ${PYTHON_HOME}/lib64/${PYTHON_VER} ${PYTHON_HOME}/Lib
			NO_DEFAULT_PATH
			CMAKE_FIND_ROOT_PATH_BOTH)
		if(PYTHON_STDLIB_DIR)
			message("Found Python stdlib dir: '${PYTHON_STDLIB_DIR}'")
			if(NOT PYTHON_NUM)
				set(PYTHON_NUM 3${PYTHON_MINOR_VER})
			endif()
			file(GLOB_RECURSE STDLIB_SUBDIR
				RELATIVE ${PYTHON_HOME}
				${PYTHON_STDLIB_DIR}/_pydecimal.py)
			get_filename_component(STDLIB_SUBDIR ${STDLIB_SUBDIR} DIRECTORY)
			message("Install Python stdlib dir: '${INSTALLDIR_SHARED}/Python${PYTHON_NUM}/${STDLIB_SUBDIR}'")
			install(DIRECTORY ${PYTHON_STDLIB_DIR}/ DESTINATION ${INSTALLDIR_SHARED}/Python${PYTHON_NUM}/${STDLIB_SUBDIR})
			add_definitions(-DPYTHON_LIBDIR="Python${PYTHON_NUM}")
			file(GLOB_RECURSE PLATFORMPATH
				RELATIVE ${PYTHON_STDLIB_DIR}
				${PYTHON_STDLIB_DIR}/*/_sysconfigdata_m.py)
			if(PLATFORMPATH MATCHES "(^[^;]*)/_sysconfigdata_m.py")
				add_definitions(-DPYTHON_LIBDIRPLAT="${CMAKE_MATCH_1}")
				message("Found separate platform python dir: '${CMAKE_MATCH_1}'")
			endif()
			add_custom_target(copy-python-files ALL
				COMMAND cmake -E copy_directory ${PYTHON_STDLIB_DIR} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Python${PYTHON_NUM}/${STDLIB_SUBDIR}
			)
			file(GLOB_RECURSE PYTHON_EXES ${PYTHON_STDLIB_DIR}/*.exe)
			foreach(python_exe ${PYTHON_EXES})
				get_filename_component(PYTHON_EXE_NAME ${python_exe} NAME)
				set(PYTHON_EXE_NAMES ${PYTHON_EXE_NAMES} ${PYTHON_EXE_NAME})
			endforeach()
		else()
			message("Warning: can't find Python std lib dir to package")
		endif()
	endif()



endif()
