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
	message("Globbed '${PYTHON_H}'")
	string(REGEX MATCH "([^;]*)[/]+include[/]+(python3.([0-9])+m)" PYTHON_VER "${PYTHON_H}")
	if(PYTHON_VER)
		set(PYTHON_HOME_DISCOVERED ${CMAKE_MATCH_1})
		set(PYTHON_VER ${CMAKE_MATCH_2})
		set(PYTHON_MINOR_VER ${CMAKE_MATCH_3})
		message("Version string: ${PYTHON_VER}")
		if(PYTHON_HOME_DISCOVERED EQUAL PYTHON_HOME)
			message("PYTHON_HOME confirmed")
		else()
			message("Warning: resetting PYTHON_HOME to ${PYTHON_HOME_DISCOVERED}")
			set(PYTHON_HOME ${PYTHON_HOME_DISCOVERED})
		endif()
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
		NAMES ${PYTHON_VER}
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
		NAMES ${PYTHON_VER}${PYTHON_DEBUG_POSTFIX}
			lib${PYTHON_VER}${PYTHON_DEBUG_POSTFIX}
			libpython${PYTHON_NUM}${PYTHON_DEBUG_POSTFIX}
			python${PYTHON_NUM}${PYTHON_DEBUG_POSTFIX}
		PATHS ${PYTHON_HOME}/lib ${PYTHON_HOME}/libs ${PYTHON_HOME}/local/lib ${PYTHON_HOME}/lib64 ${PYTHON_HOME}/local/lib64
		PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE}
		NO_DEFAULT_PATH
		CMAKE_FIND_ROOT_PATH_BOTH)

	if(WIN32)
		if (NOT PYTHON_LIBRARY_DEBUG)
			#FIXME:
			# Appveyor does not have the debug libraries. Point to our copy that we have put into GIT. This issue will be fixed on next Image Release
			# Remove this and python37_d.lib from GIT when the Appveyor image catches up.
			message("Hack to work around missing Python debug library")

			if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")	# 64 Bit
				set(platform "x64")
			else()
				set(platform "x86")
			endif()

			set(PYTHON_LIBRARY_DEBUG "${CMAKE_SOURCE_DIR}/PyPort/${platform}/python37_d.lib")
		endif()
	endif()

	add_library(python_target_d UNKNOWN IMPORTED)
	set_property(TARGET python_target_d PROPERTY IMPORTED_LOCATION "${PYTHON_LIBRARY_DEBUG}")


	#set a variable to use for linking
	set(PYTHON_LIBRARIES debug python_target_d optimized python_target )

endif()
