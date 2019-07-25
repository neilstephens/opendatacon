#	opendatacon
 #
 #	Copyright (c) 2014:
 #
 #		DCrip3fJguWgVCLrZFfA7sIGgvx1Ou3fHfCxnrz4svAi
 #		yxeOtDhDCXf1Z4ApgXvX5ahqQmzRfJ2DoX8S05SqHA==
 #	
 #	Licensed under the Apache License, Version 2.0 (the "License");
 #	you may not use this file except in compliance with the License.
 #	You may obtain a copy of the License at
 #	
 #		http://www.apache.org/licenses/LICENSE-2.0
 #
 #	Unless required by applicable law or agreed to in writing, software
 #	distributed under the License is distributed on an "AS IS" BASIS,
 #	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 #	See the License for the specific language governing permissions and
 #	limitations under the License.
 # 
project(DNP3Port)
cmake_minimum_required(VERSION 2.8)
file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def)

# Locate opendnp3
option(USE_DNP3_SUBMODULE "Use git submodule to download and build opendnp3 libraries" ON)
set(DNP3_HOME_INSTRUCTIONS "Choose the location of opendnp3: where it was installed - there should be lib and include directories within")
if(USE_DNP3_SUBMODULE)
	set(DNP3_SOURCE "${CMAKE_SOURCE_DIR}/submodules/dnp3")
	set(DNP3_BUILD "${CMAKE_BINARY_DIR}/submodules/dnp3")
	mark_as_advanced(FORCE DNP3_SOURCE)
	mark_as_advanced(FORCE DNP3_BUILD)
	set(DNP3_HOME "${DNP3_BUILD}/install" CACHE PATH ${DNP3_HOME_INSTRUCTIONS} FORCE)
	set(
		DNP3_CMAKE_OPTS
			-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
			-DSTATICLIBS=1
			-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
			-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
			-DCMAKE_INSTALL_PREFIX=${DNP3_HOME}/
			-DCMAKE_DEBUG_POSTFIX=${CMAKE_DEBUG_POSTFIX}
			-DASIO_HOME=${ASIO_HOME}/include
		CACHE STRING "cmake options to use when building DNP3 submodule"
	)
	if(NOT EXISTS "${DNP3_SOURCE}/.git")
		execute_process(COMMAND git submodule update --init -- submodules/dnp3
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	endif()
	if(NOT EXISTS "${DNP3_BUILD}")
		file(MAKE_DIRECTORY "${DNP3_BUILD}")
	endif()
	if(DEFINED ${CMAKE_GENERATOR_PLATFORM})
		set(PLATFORM_OPT "-A${CMAKE_GENERATOR_PLATFORM}")
	elseif(DEFINED ${CMAKE_VS_PLATFORM_NAME})
		set(PLATFORM_OPT "-A${CMAKE_VS_PLATFORM_NAME}")
	else()
		set(PLATFORM_OPT "")
	endif()
	message("${CMAKE_COMMAND} ${DNP3_CMAKE_OPTS} -G${CMAKE_GENERATOR} ${PLATFORM_OPT} ${DNP3_SOURCE}")
	execute_process(
		COMMAND ${CMAKE_COMMAND} ${DNP3_CMAKE_OPTS} -G${CMAKE_GENERATOR} ${PLATFORM_OPT} ${DNP3_SOURCE}
		WORKING_DIRECTORY "${DNP3_BUILD}"
		RESULT_VARIABLE EXEC_RESULT
	)
	if(EXEC_RESULT)
		message( FATAL_ERROR "Failed to run cmake for DNP3 submodule, exiting")
	endif()
	set(CONFIGS "${CMAKE_BUILD_TYPE}")
	if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Rel.*")
		list(APPEND CONFIGS "Release")
	endif()
	if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Deb.*")
		list(APPEND CONFIGS "Debug")
	endif()
	foreach(CONF ${CONFIGS})
		message("Building DNP3 dependency")
		execute_process(
			COMMAND ${CMAKE_COMMAND} --build ${DNP3_BUILD} --config ${CONF} --target install
			WORKING_DIRECTORY "${DNP3_BUILD}"
			RESULT_VARIABLE EXEC_RESULT
		)
		if(EXEC_RESULT)
			message( FATAL_ERROR "Failed to build DNP3 submodule, exiting")
		endif()
	endforeach()
	add_custom_target( build_dnp3
		WORKING_DIRECTORY "${DNP3_BUILD}"
		COMMAND ${CMAKE_COMMAND} --build ${DNP3_BUILD} --config $<CONFIG> --target install
	)
else()
	if(DEFINED DNP3_HOME)
		set(DNP3_HOME ${DNP3_HOME} CACHE PATH ${DNP3_HOME_INSTRUCTIONS})
	else()
		set(DNP3_HOME "/usr" CACHE PATH ${DNP3_HOME_INSTRUCTIONS})
	endif()
endif()

#find dnp3 headers
find_path(DNP3_INCLUDE_PATH opendnp3/LogLevels.h
	PATHS ${DNP3_HOME}/include ${DNP3_HOME}/local/include
	NO_DEFAULT_PATH
	CMAKE_FIND_ROOT_PATH_BOTH)
mark_as_advanced(FORCE DNP3_INCLUDE_PATH)

foreach( component openpal opendnp3 asiopal asiodnp3 )
	# Look for regular libraries
	find_library(${component}_lib NAMES ${component} PATHS ${DNP3_HOME}/lib ${DNP3_HOME}/local/lib NO_DEFAULT_PATH CMAKE_FIND_ROOT_PATH_BOTH)
	add_library(${component}_target UNKNOWN IMPORTED)
	set_property(TARGET ${component}_target PROPERTY IMPORTED_LOCATION "${${component}_lib}")
	# Look for debug libraries
	find_library(${component}_dlib NAMES ${component}${CMAKE_DEBUG_POSTFIX} PATHS ${DNP3_HOME}/lib ${DNP3_HOME}/local/lib NO_DEFAULT_PATH CMAKE_FIND_ROOT_PATH_BOTH)
	add_library(${component}_dtarget UNKNOWN IMPORTED)
	set_property(TARGET ${component}_dtarget PROPERTY IMPORTED_LOCATION "${${component}_dlib}")
	#set a variable to use for linking
	set(${component}_libs debug ${component}_dtarget optimized ${component}_target )

	#hide (in non-advanced mode) the library paths in cmake guis since they are derrived
	mark_as_advanced(FORCE ${component}_lib ${component}_dlib)

	#include in install/packaging
	file_w_symlinks(file_w_links "${${component}_lib}") #resolve symlinks
	install(FILES ${file_w_links} DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Release" "RelWithDebInfo" "MinSizeRel")
	file_w_symlinks(file_w_links "${${component}_dlib}") #resolve symlinks
	install(FILES ${file_w_links} DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Debug")

endforeach()

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
if(TARGET build_dnp3)
	add_dependencies(${PROJECT_NAME} build_dnp3)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE "${DNP3_INCLUDE_PATH}")
target_link_libraries(${PROJECT_NAME} ${asiodnp3_libs} ${asiopal_libs} ${opendnp3_libs} ${openpal_libs} ODC)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)

if(PACKAGE_SYSTEM_LIBS)
	install(CODE
	"
		set(BUNDLE_DEPS_LIST \${BUNDLE_DEPS_LIST}
			\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_MODULES}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
		)
	")
endif()
