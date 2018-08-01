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
	mark_as_advanced(FORCE DNP3_SOURCE)
	set(DNP3_HOME "${DNP3_SOURCE}/build/install" CACHE PATH ${DNP3_HOME_INSTRUCTIONS} FORCE)
	set(
		DNP3_CMAKE_OPTS
			-DCMAKE_INSTALL_PREFIX=${DNP3_HOME}/
			-DASIO_HOME=${ASIO_HOME}/include
			-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
			\"-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}\"
		CACHE STRING "cmake options to use when building DNP3 submodule"
	)
	if(NOT EXISTS "${DNP3_SOURCE}/.git")
		execute_process(COMMAND git submodule update --init -- submodules/dnp3
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	endif()
	if(NOT EXISTS "${DNP3_SOURCE}/build")
		file(MAKE_DIRECTORY "${DNP3_SOURCE}/build")
	endif()
	execute_process(
		COMMAND ${CMAKE_COMMAND} ${DNP3_CMAKE_OPTS} ${DNP3_SOURCE}
		WORKING_DIRECTORY "${DNP3_SOURCE}/build"
		RESULT_VARIABLE EXEC_RESULT
	)
	if(EXEC_RESULT)
		message( FATAL_ERROR "Failed to run cmake for DNP3 submodule, exiting")
	endif()
	execute_process(
		COMMAND ${CMAKE_COMMAND} --build ${DNP3_SOURCE}/build --target install
		WORKING_DIRECTORY "${DNP3_SOURCE}/build"
		RESULT_VARIABLE EXEC_RESULT
	)
	if(EXEC_RESULT)
		message( FATAL_ERROR "Failed to build DNP3 submodule, exiting")
	endif()
	add_custom_target( build_dnp3
		WORKING_DIRECTORY "${DNP3_SOURCE}/build"
		COMMAND ${CMAKE_COMMAND} --build ${DNP3_SOURCE}/build --target install
	)
else()
	if(DEFINED DNP3_HOME)
		set(DNP3_HOME ${DNP3_HOME} CACHE PATH ${DNP3_HOME_INSTRUCTIONS})
	else()
		set(DNP3_HOME "/usr/local" CACHE PATH ${DNP3_HOME_INSTRUCTIONS})
	endif()
endif()

foreach( component openpal opendnp3 asiopal asiodnp3 )
	# Look for regular libraries
	find_library(${component}_lib NAMES ${component} PATHS ${DNP3_HOME}/lib NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
	add_library(${component}_target UNKNOWN IMPORTED)
	set_property(TARGET ${component}_target PROPERTY IMPORTED_LOCATION "${${component}_lib}")
	# Look for debug libraries
	find_library(${component}_dlib NAMES ${component}${CMAKE_DEBUG_POSTFIX} PATHS ${DNP3_HOME}/lib NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
	add_library(${component}_dtarget UNKNOWN IMPORTED)
	set_property(TARGET ${component}_dtarget PROPERTY IMPORTED_LOCATION "${${component}_dlib}")
	#set a variable to use for linking
	set(${component}_libs debug ${component}_dtarget optimized ${component}_target )

	#hide (in non-advanced mode) the library paths in cmake guis since they are derrived
	mark_as_advanced(FORCE ${component}_lib ${component}_dlib)

	#include in install/packaging
	get_filename_component(_resolvedFile "${${component}_lib}" REALPATH) #check for symolic link
	if(${_resolvedFile} STREQUAL ${${component}_lib})
		#not a link - just install file
		install(FILES "${${component}_lib}" DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Release" "RelWithDebInfo" "MinSizeRel")
	else()
		#is a link -install link and file
		install(FILES "${${component}_lib}" "${_resolvedFile}" DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Release" "RelWithDebInfo" "MinSizeRel")
	endif()
	get_filename_component(_resolvedFile "${${component}_dlib}" REALPATH) #check for symolic link
	if(${_resolvedFile} STREQUAL ${${component}_dlib})
		#not a link - just install file
		install(FILES "${${component}_dlib}" DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Debug")
	else()
		#is a link -install link and file
		install(FILES "${${component}_dlib}" "${_resolvedFile}" DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Debug")
	endif()
endforeach()

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
if(TARGET build_dnp3)
	add_dependencies(${PROJECT_NAME} build_dnp3)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE "${DNP3_HOME}/include")
target_link_libraries(${PROJECT_NAME} ODC ${asiodnp3_libs} ${asiopal_libs} ${opendnp3_libs} ${openpal_libs})

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)
