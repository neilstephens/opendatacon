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
project(opendatacon)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(src SRC_LIST)

# various optional libraries and projects
option(FULL "Build all optional projects (DNP3Port, JSONPort, Tests, WebUI)" ON)
option(TEST "Build tests" OFF)
option(WEBUI "Build the http(s) web user interface" OFF)
option(DNP3PORT "Build DNP3 Port" OFF)
option(JSONPORT "Build DNP3 Port" OFF)

# other options off-by-default that you can enable
option(WERROR "Set all warnings to errors" OFF)
option(COVERAGE "Builds the libraries with coverage info for gcov" OFF)

if(FULL)
	set(TEST ON)
	set(WEBUI ON)
	set(DNP3PORT ON)
	set(JSONPORT ON)
endif()

if(WIN32)
	set_property(GLOBAL PROPERTY USE_FOLDERS ON) #allows the creation of solution folders

	# for ASIO
	add_definitions(-D_WIN32_WINNT=0x0501)
	add_definitions(-DASIO_HAS_STD_SYSTEM_ERROR)

	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /MP")
else()	
	set(PTHREAD pthread)
	set(DL dl)

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

# required for ASIO in C++11 only mode
add_definitions(-DASIO_STANDALONE)

# Second, for multi-config builds (e.g. msvc)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
	string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${PROJECT_SOURCE_DIR}/${OUTPUTCONFIG}" )
	set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${PROJECT_SOURCE_DIR}/${OUTPUTCONFIG}" )
	set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${PROJECT_SOURCE_DIR}/${OUTPUTCONFIG}" )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

if(CMAKE_BUILD_TYPE MATCHES "Debug")
	if(CMAKE_CXX_COMPILER MATCHES "rpi")
		set(MY_BUILD_CONFIG "Debug-rpi")
	elseif(CMAKE_CXX_COMPILER MATCHES "RHEL65")
		set(MY_BUILD_CONFIG "Debug-RHEL65")
	else()
		set(MY_BUILD_CONFIG "Debug")
	endif()
else()
	if(CMAKE_CXX_COMPILER MATCHES "rpi")
		set(MY_BUILD_CONFIG "Release-rpi")
	elseif(CMAKE_CXX_COMPILER MATCHES "RHEL65")
		set(MY_BUILD_CONFIG "Release-RHEL65")
	else()
		set(MY_BUILD_CONFIG "Release")
	endif()
endif()

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
if(TEST)
	add_subdirectory(tests)
endif()
if(MODBUSPORT)
	add_subdirectory(ModbusPort)
endif()

# skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  TRUE)

# when building use the install RPATH already
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

# the RPATH to be used when installing
SET(CMAKE_INSTALL_RPATH
	"\$ORIGIN:"
	"\$ORIGIN/libs:"
	"\$ORIGIN/JSON:"
	"\$ORIGIN/ODC:"
	"\$ORIGIN/DNP3Port:"
	"\$ORIGIN/JSONPort:"
	"\$ORIGIN/ModbusPort:"
	"\$ORIGIN/WebUI")

# don't add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

include_directories(
	"include"
	"JSON"
	"${DNP3_HOME}/cpp/libs/include/"
	"${ASIO_HOME}/include/"
	"${TCLAP_HOME}/include/"
)
link_directories("${DNP3_HOME}/${MY_BUILD_CONFIG}")
link_libraries(ODC JSON asiopal openpal asiodnp3 opendnp3 ${PTHREAD} ${DL})
add_executable(${PROJECT_NAME} ${SRC_LIST})
