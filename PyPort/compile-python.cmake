if(USE_PYTHON_SUBMODULE)
	set(PYTHON_SOURCE "${CMAKE_SOURCE_DIR}/submodules/python-cmake-buildsystem")
	set(PYTHON_BUILD "${CMAKE_BINARY_DIR}/submodules/python-cmake-buildsystem")
	mark_as_advanced(FORCE PYTHON_SOURCE)
	mark_as_advanced(FORCE PYTHON_BUILD)
	set(PYTHON_HOME "${PYTHON_BUILD}/install" CACHE PATH ${PYTHON_HOME_INSTRUCTIONS} FORCE)
	set(
		PYTHON_CMAKE_OPTS
			-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
			-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
			-DCMAKE_INSTALL_PREFIX=${PYTHON_HOME}/
			-DBUILD_EXTENSIONS_AS_BUILTIN=ON
			-DCMAKE_DEBUG_POSTFIX=${CMAKE_DEBUG_POSTFIX}
		CACHE STRING "cmake options to use when building using submodule to build Python"
	)
	if(NOT EXISTS "${PYTHON_SOURCE}/.git")
		execute_process(COMMAND git submodule update --init -- submodules/python-cmake-buildsystem
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	endif()
	if(NOT EXISTS "${PYTHON_BUILD}")
		file(MAKE_DIRECTORY "${PYTHON_BUILD}")
	endif()
	if(DEFINED ${CMAKE_GENERATOR_PLATFORM})
		set(PLATFORM_OPT "-A${CMAKE_GENERATOR_PLATFORM}")
	elseif(DEFINED ${CMAKE_VS_PLATFORM_NAME})
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
			COMMAND ${CMAKE_COMMAND} --build ${PYTHON_BUILD} --config ${CONF} --target install
			WORKING_DIRECTORY "${PYTHON_BUILD}"
			RESULT_VARIABLE EXEC_RESULT
		)
		if(EXEC_RESULT)
			message( FATAL_ERROR "Failed to build Python submodule (exit code ${EXEC_RESULT}), exiting")
		endif()
	endforeach()
	add_custom_target( build_python
		WORKING_DIRECTORY "${PYTHON_BUILD}"
		COMMAND ${CMAKE_COMMAND} --build ${PYTHON_BUILD} --config $<CONFIG> --target install
	)

	include(${PYTHON_BUILD}/share/python3.6/PythonConfig.cmake)
	set(PYTHON_LIBRARIES libpython-static)
	install(DIRECTORY ${PYTHON_BUILD}/install/lib/python3.6 DESTINATION ${INSTALLDIR_LIBS})
	add_definitions(-DPYTHON_LIBDIR="python3.6")
endif()
