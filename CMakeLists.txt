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
if(WIN32)
	cmake_minimum_required(VERSION 3.4)
else()
	cmake_minimum_required(VERSION 2.8)
	set(BUILD_TYPE_INSTRUCTIONS "Choose the type of build, default Release, other options: Debug RelWithDebInfo MinSizeRel.")
	if(DEFINED CMAKE_BUILD_TYPE)
		set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING ${BUILD_TYPE_INSTRUCTIONS})
	else()
		set(CMAKE_BUILD_TYPE Release CACHE STRING ${BUILD_TYPE_INSTRUCTIONS})
	endif()
endif()

if(POLICY CMP0048)
	cmake_policy(SET CMP0048 OLD)
endif()

project(opendatacon_suite C CXX)

find_package(Git)
if(GIT_FOUND)
 	message("-- git found: ${GIT_EXECUTABLE}, determining source version")
	execute_process(
		COMMAND git describe --long --match "[0-9]*\\.[0-9]*\\.[0-9]" --dirty
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_DESCRIBE
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
 	#message(" Git describe: ${GIT_DESCRIBE}")
	string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)-[0-9]+-([^-]+)-?([^-]*)$" GIT_REPO_MATCH ${GIT_DESCRIBE})
 	#message(" Version: ${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3} ${CMAKE_MATCH_4}")

	set(ODC_MAJOR_VERSION ${CMAKE_MATCH_1})
	set(ODC_MINOR_VERSION ${CMAKE_MATCH_2})
	set(ODC_MICRO_VERSION ${CMAKE_MATCH_3})
	set(ODC_VERSION "${ODC_MAJOR_VERSION}.${ODC_MINOR_VERSION}.${ODC_MICRO_VERSION}")

	set(GIT_REPO_COMMIT ${CMAKE_MATCH_4})
	set(GIT_REPO_DIRTY ${CMAKE_MATCH_5})
	message("-- opendatacon version: ${ODC_VERSION} ${GIT_REPO_COMMIT} ${GIT_REPO_DIRTY}")

	configure_file (
		"${CMAKE_SOURCE_DIR}/include/opendatacon/Version.h.in"
		"${CMAKE_SOURCE_DIR}/include/opendatacon/Version.h"
  	)

endif()


# various optional libraries and projects
option(FULL "Build all optional projects (DNP3Port, JSONPort, ModbusPort, SimPort, Tests, ConsoleUI, WebUI)" OFF)

set(TESTS OFF CACHE BOOL "Build tests"					)
set(WEBUI OFF CACHE BOOL "Build the http(s) web user interface"	)
set(DNP3PORT OFF CACHE BOOL "Build DNP3 Port"				)
set(JSONPORT OFF CACHE BOOL "Build JSON Port"				)
set(MODBUSPORT OFF CACHE BOOL "Build Modbus Port"			)
set(SIMPORT OFF CACHE BOOL "Build Simulation Port"			)
set(CONSOLEUI OFF CACHE BOOL "Build the console user interface"	)

# other options off-by-default that you can enable
option(WERROR "Set all warnings to errors" OFF)
option(COVERAGE "Builds the libraries with coverage info for gcov" OFF)

if(FULL)
	set(TESTS ON CACHE BOOL "Build tests"				   	FORCE)
	set(WEBUI ON CACHE BOOL "Build the http(s) web user interface"	FORCE)
	set(DNP3PORT ON CACHE BOOL "Build DNP3 Port"			   	FORCE)
	set(JSONPORT ON CACHE BOOL "Build JSON Port"			   	FORCE)
	set(MODBUSPORT ON CACHE BOOL "Build Modbus Port"		   	FORCE)
	set(SIMPORT ON CACHE BOOL "Build Simulation Port"		   	FORCE)
	set(CONSOLEUI ON CACHE BOOL "Build the console user interface"	FORCE)
endif()

# IDE configuration
set_property(GLOBAL PROPERTY USE_FOLDERS ON) #allows the creation of solution folders

# Platform configuration
if(WIN32)
	# for ASIO
	add_definitions(-D_WIN32_WINNT=0x0502)	
	add_definitions(-DASIO_HAS_STD_SYSTEM_ERROR)
	add_definitions(-DVC_EXTRALEAN)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
	
	# Auto generate def files that export all symbols of STATIC libraries
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
	
	# UNICODE support
	add_definitions(-DUNICODE -D_UNICODE)
	
	# Postfix for debug builds
	SET(CMAKE_DEBUG_POSTFIX "d")

	# Install directories
	set(INSTALLDIR_BINS ".")
	set(INSTALLDIR_INCLUDES "include")
	set(INSTALLDIR_LIBS "lib")
	set(INSTALLDIR_MODULES ".")
endif()
if(UNIX)
	set(DL dl)
	set(RPATH_BASE $ORIGIN)

	# Install directories
	set(INSTALLDIR_BINS "bin")
	set(INSTALLDIR_INCLUDES "include")
	set(INSTALLDIR_LIBS "lib")
	set(INSTALLDIR_MODULES "plugin")
endif()
if(APPLE)
	set(CMAKE_MACOSX_RPATH 1)
	set(RPATH_BASE @executable_path)

	# Install directories
	set(INSTALLDIR_BINS "bin")
	set(INSTALLDIR_INCLUDES "include")
	set(INSTALLDIR_LIBS "lib")
	set(INSTALLDIR_MODULES "plugin")
endif()

# Compiler configuration

add_definitions(-DASIO_STANDALONE) # required for ASIO in C++11 only mode
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	# using Visual Studio C++
	message("CMAKE configuring opendatacon for Visual Studio C++" )
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /MP")

elseif(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
	# using Intel C++
	message(WARNING "opendatacon has not been tested using Intel compiler" )
	
else()#if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
	# using Clang or GNU compilers
	message("-- configuring opendatacon for  Clang or GNU compilers" )
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++14 -pedantic")

	# different release and debug flags
	set(CMAKE_CXX_FLAGS_RELEASE "-O3")
	set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g")

	if(COVERAGE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -g -O0")
		#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage -g -O0")
	endif()

	if (WERROR)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
	endif()

	#package the c++ standard library
	execute_process(
		COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libstdc++.so.6
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE std_lib
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	#hide (in non-advanced mode) the library paths in cmake guis since they are derrived
	mark_as_advanced(FORCE std_lib)
	get_filename_component(_resolvedFile "${std_lib}" REALPATH) #check for symolic link
	if(${_resolvedFile} STREQUAL ${std_lib})
		#not a link - just install file
		install(FILES "${std_lib}" DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
	else()
		#is a link -install link and file
		install(FILES "${std_lib}" "${_resolvedFile}" DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
	endif()
endif()

find_package(Threads REQUIRED)
if(CMAKE_USE_PTHREADS_INIT)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

# the RPATH to be used when installing - allowing local libraries to be found for standalone installs,
# as well as installed locations
SET(CMAKE_INSTALL_RPATH "${RPATH_BASE}"
	"${RPATH_BASE}/${INSTALLDIR_LIBS}"
	"${RPATH_BASE}/${INSTALLDIR_MODULES}"
	"${RPATH_BASE}/../${INSTALLDIR_LIBS}"
	"${RPATH_BASE}/../${INSTALLDIR_MODULES}"
	"${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_LIBS}"
	"${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_MODULES}")
# when building use the install RPATH already
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
# add the automatically determined parts of the RPATH - for linked libs outside build tree
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build" )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build/${INSTALLDIR_LIBS}" )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build" )

# Second, for multi-config builds (e.g. msvc)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
	string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}" )
	set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}" )
	set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}" )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

# Locate opendnp3 - this can be moved into DNP3Port when the other projects are decoupled from opendnp3
set(DNP3_HOME_INSTRUCTIONS "Choose the location of opendnp3: where it was installed - there should be lib and include directories within")
if(DEFINED DNP3_HOME)
	set(DNP3_HOME ${DNP3_HOME} CACHE PATH ${DNP3_HOME_INSTRUCTIONS})
else()
	set(DNP3_HOME "/usr/local" CACHE PATH ${DNP3_HOME_INSTRUCTIONS})
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

set(ASIO_HOME_INSTRUCTIONS "Choose the location of asio header library: where it was installed - there should be an include directory within")
if(DEFINED ASIO_HOME)
	set(ASIO_HOME ${ASIO_HOME} CACHE PATH ${ASIO_HOME_INSTRUCTIONS})
else()
	set(ASIO_HOME "/usr/local" CACHE PATH ${ASIO_HOME_INSTRUCTIONS})
endif()

message("add subdir JSON")
add_subdirectory(JSON)
message("add subdir ODC")
add_subdirectory(ODC)
message("add subdir opendatacon")
add_subdirectory(opendatacon)
if(DNP3PORT)
	message("add subdir DNP3Port")
	add_subdirectory(DNP3Port)
endif()
if(JSONPORT)
	message("add subdir JSONPort")
	add_subdirectory(JSONPort)
endif()
if(WEBUI)
	message("add subdir WebUI")
	add_subdirectory(WebUI)
endif()
if(CONSOLEUI)
	message("add subdir ConsoleUI")
	add_subdirectory(ConsoleUI)
endif()
if(TESTS)
	message("add subdir tests")
	enable_testing()
	add_subdirectory(tests)
	add_test(DNP3Port_tests DNP3Port_tests)
	add_test(ODC_tests ODC_tests)
endif()
if(MODBUSPORT)
	message("add subdir ModbusPort")
	add_subdirectory(ModbusPort)
endif()
if(SIMPORT)
	message("add subdir SimPort")
	add_subdirectory(SimPort)
endif()	
message("add subdir install")
add_subdirectory("install")
