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
endif()

if(POLICY CMP0048)
	cmake_policy(SET CMP0048 OLD)
endif()

project(opendatacon_suite CXX)

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
option(FULL "Build all optional projects (DNP3Port, JSONPort, ModbusPort, SimPort, Tests, ConsoleUI, WebUI)" ON)
option(TESTS "Build tests" OFF)
option(WEBUI "Build the http(s) web user interface" OFF)
option(DNP3PORT "Build DNP3 Port" OFF)
option(JSONPORT "Build JSON Port" OFF)
option(MODBUSPORT "Build Modbus Port" OFF)
option(SIMPORT "Build Simulation Port" OFF)
option(CONSOLEUI "Build the console user interface" OFF)

# other options off-by-default that you can enable
option(WERROR "Set all warnings to errors" OFF)
option(COVERAGE "Builds the libraries with coverage info for gcov" OFF)

if(FULL)
	set(TESTS ON)
	set(WEBUI ON)
	set(CONSOLEUI ON)
	set(DNP3PORT ON)
	set(JSONPORT ON)
	set(MODBUSPORT ON)
	set(SIMPORT ON)
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
	set(INSTALLDIR_MODULES "plugin")
endif()
if(UNIX)
	set(PTHREAD pthread)
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
	set(CMAKE_CXX_FLAGS "-Wall -std=c++11 -pedantic")

	# different release and debug flags
	set(CMAKE_CXX_FLAGS_RELEASE "-O3")
	set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

	if(COVERAGE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -g -O0")
		#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage -g -O0")
	endif()

	if (WERROR)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
	endif()
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
foreach( component openpal opendnp3 asiopal asiodnp3 )
	# Look for regular libraries
	find_library(${component}_lib NAMES ${component} PATHS ${DNP3_HOME}/lib)
	add_library(${component}_target UNKNOWN IMPORTED)
	set_property(TARGET ${component}_target PROPERTY IMPORTED_LOCATION "${${component}_lib}")
	# Look for debug libraries
	find_library(${component}_dlib NAMES ${component}${CMAKE_DEBUG_POSTFIX} PATHS ${DNP3_HOME}/lib)
	add_library(${component}_dtarget UNKNOWN IMPORTED)
	set_property(TARGET ${component}_dtarget PROPERTY IMPORTED_LOCATION "${${component}_dlib}")
	#set a variable to use for linking
	set(${component}_libs debug ${component}_dtarget optimized ${component}_target )

	#include in install/packaging
	get_filename_component(_resolvedFile "${${component}_lib}" REALPATH) #check for symolic link
	if(${_resolvedFile} STREQUAL ${${component}_lib})
		#not a link - just install file
		install(FILES "${${component}_lib}" DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Release")
	else()
		#is a link -install link and file
		install(FILES "${${component}_lib}" "${_resolvedFile}" DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Release")
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

add_subdirectory(JSON)
add_subdirectory(ODC)
add_subdirectory(opendatacon)
if(DNP3PORT)
	add_subdirectory(DNP3Port)
endif()
if(JSONPORT)
	add_subdirectory(JSONPort)
endif()
if(WEBUI)
	add_subdirectory(WebUI)
endif()
if(CONSOLEUI)
	add_subdirectory(ConsoleUI)
endif()
if(TESTS)
	add_subdirectory(tests)
endif()
if(MODBUSPORT)
	add_subdirectory(ModbusPort)
endif()
if(SIMPORT)
	add_subdirectory(SimPort)
endif()	
add_subdirectory("install")
