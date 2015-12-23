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
cmake_minimum_required(VERSION 3.4)
project(opendatacon CXX)

# various optional libraries and projects
option(FULL "Build all optional projects (DNP3Port, JSONPort, Tests, WebUI)" ON)
option(TESTS "Build tests" OFF)
option(WEBUI "Build the http(s) web user interface" OFF)
option(DNP3PORT "Build DNP3 Port" OFF)
option(JSONPORT "Build JSON Port" OFF)
option(MODBUSPORT "Build Modbus Port" OFF)

# other options off-by-default that you can enable
option(WERROR "Set all warnings to errors" OFF)
option(COVERAGE "Builds the libraries with coverage info for gcov" OFF)

if(FULL)
	set(TESTS ON)
	set(WEBUI ON)
	set(DNP3PORT ON)
	set(JSONPORT ON)
	set(MODBUSPORT ON)
endif()

# IDE configuration
set_property(GLOBAL PROPERTY USE_FOLDERS ON) #allows the creation of solution folders

# Platform configuration
if(WIN32)
	# for ASIO
	add_definitions(-D_WIN32_WINNT=0x0501)
	add_definitions(-DASIO_HAS_STD_SYSTEM_ERROR)
	
	# Auto generate def files that export all symbols of STATIC libraries
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
if(UNIX)
	set(PTHREAD pthread)
	set(DL dl)	
endif()
if(APPLE)
	set(CMAKE_MACOSX_RPATH 1)
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
	message("CMAKE configuring opendatacon for  Clang or GNU compilers" )
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

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" )

# Second, for multi-config builds (e.g. msvc)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
	string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}" )
	set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}" )
	set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}" )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

add_subdirectory(JSON)
add_subdirectory(ODC)
if(DNP3PORT)
	add_subdirectory(DNP3Port)
endif()
if(JSONPORT)
	add_subdirectory(JSONPort)
endif()
if(WEBUI)
	add_subdirectory(WebUI)
endif()
if(TESTS)
	add_subdirectory(tests)
endif()
if(MODBUSPORT)
	add_subdirectory(ModbusPort)
endif()

# main opendatacon executable
file(GLOB SRC_LIST src/*.cpp src/*.h include/opendatacon/*.h)
include_directories(
	"include"
	"JSON"
	"${DNP3_HOME}/include"
	"${ASIO_HOME}/include"
	"${TCLAP_HOME}/include"
)
link_directories("${DNP3_HOME}/lib")

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} ODC JSON asiopal openpal asiodnp3 opendnp3 ${PTHREAD} ${DL})

install(DIRECTORY "include/opendatacon" DESTINATION "include")
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "bin")
