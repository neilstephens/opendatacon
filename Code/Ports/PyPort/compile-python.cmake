if(USE_PYTHON_SUBMODULE)
	set(PYTHON_SOURCE "${CMAKE_SOURCE_DIR}/Code/submodules/python-cmake-buildsystem")
	set(PYTHON_BUILD "${CMAKE_BINARY_DIR}/Code/submodules/python-cmake-buildsystem")
	mark_as_advanced(FORCE PYTHON_SOURCE)
	mark_as_advanced(FORCE PYTHON_BUILD)
	set(PYTHON_HOME "${PYTHON_BUILD}/install" CACHE PATH ${PYTHON_HOME_INSTRUCTIONS} FORCE)
	if(APPLE)
		set(PYTHON_LINK_OPT "-DCMAKE_STATIC_LINKER_FLAGS=-framework CoreFoundation")
	endif()
	set(
		PYTHON_CMAKE_OPTS
			-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
			-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
			-DCMAKE_INSTALL_PREFIX=${PYTHON_HOME}/
			-DBUILD_EXTENSIONS_AS_BUILTIN=ON
			-DCMAKE_DEBUG_POSTFIX=${CMAKE_DEBUG_POSTFIX}
			-DCMAKE_CROSSCOMPILING_EMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}
			-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
			${PYTHON_LINK_OPT}
		CACHE STRING "cmake options to use when building using submodule to build Python"
	)
	if(NOT EXISTS "${PYTHON_SOURCE}/.git")
		execute_process(COMMAND git submodule update --init -- Code/submodules/python-cmake-buildsystem
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	endif()
	if(NOT EXISTS "${PYTHON_BUILD}")
		file(MAKE_DIRECTORY "${PYTHON_BUILD}")
	endif()
	if(CMAKE_GENERATOR_PLATFORM)
		set(PLATFORM_OPT "-A${CMAKE_GENERATOR_PLATFORM}")
	elseif(CMAKE_VS_PLATFORM_NAME)
		set(PLATFORM_OPT "-A${CMAKE_VS_PLATFORM_NAME}")
	else()
		set(PLATFORM_OPT "")
	endif()
	execute_process(
		COMMAND ${CMAKE_COMMAND} ${PYTHON_CMAKE_OPTS} -G${CMAKE_GENERATOR} ${PLATFORM_OPT} ${PYTHON_SOURCE}
		WORKING_DIRECTORY "${PYTHON_BUILD}"
		RESULT_VARIABLE EXEC_RESULT
	)
	if(EXEC_RESULT)
		message( FATAL_ERROR "Failed to run cmake for Python submodule, exiting")
	endif()
	set(CONFIGS "${CMAKE_BUILD_TYPE}")
	if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Rel.*")
		list(APPEND CONFIGS "Release")
	endif()
	if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Deb.*")
		list(APPEND CONFIGS "Debug")
	endif()
	foreach(CONF ${CONFIGS})
		message("Building Python dependency")
		execute_process(
			COMMAND ${CMAKE_COMMAND} --build ${PYTHON_BUILD} --config ${CONF} --parallel 8 --target install
			WORKING_DIRECTORY "${PYTHON_BUILD}"
			RESULT_VARIABLE EXEC_RESULT
		)
		if(EXEC_RESULT)
			message( FATAL_ERROR "Failed to build Python submodule (exit code ${EXEC_RESULT}), exiting")
		endif()
	endforeach()
	add_custom_target( build_python
		WORKING_DIRECTORY "${PYTHON_BUILD}"
		COMMAND ${CMAKE_COMMAND} --build ${PYTHON_BUILD} --config $<CONFIG> --parallel 8 --target install
	)

	include(${PYTHON_BUILD}/share/python3.6/PythonConfig.cmake)
	set(PYTHON_LIBRARIES libpython-static)
	install(DIRECTORY ${PYTHON_BUILD}/install/lib/python3.6/ DESTINATION ${INSTALLDIR_SHARED}/Python36/lib/python3.6/)
	add_definitions(-DPYTHON_LIBDIR="Python36")
	add_custom_target(copy-python-files ALL
		COMMAND cmake -E copy_directory ${PYTHON_BUILD}/install/lib/python3.6 ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Python36
		DEPENDS build_python
	)
	file(GLOB_RECURSE PYTHON_EXES ${PYTHON_BUILD}/install/lib/python3.6/*.exe)
	foreach(python_exe ${PYTHON_EXES})
		get_filename_component(PYTHON_EXE_NAME ${python_exe} NAME)
		set(PYTHON_EXE_NAMES ${PYTHON_EXE_NAMES} ${PYTHON_EXE_NAME})
	endforeach()
endif()
