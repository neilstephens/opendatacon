#	opendatacon
 #
 #	Copyright (c) 2020:
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
 project(SimpleWebServer)

# Locate Simple Web Server
option(USE_SIMPLEWEB_SUBMODULE "Use git submodule to download Simple-Web-Server source" ON)
set(SIMPLEWEB_HOME_INSTRUCTIONS "Choose the location of Simple-Web-Server source: there should be the main CMakeLists.txt within")

set(SIMPLEWEB_SSL OFF CACHE BOOL "Build SSL client/server support in Simple-Web-Server (requires ODC_ASIO_SSL)")
if(FULL)
	set(SIMPLEWEB_SSL ON CACHE BOOL "Build SSL client/server support in Simple-Web-Server (requires ODC_ASIO_SSL)" FORCE)
endif()
if(SIMPLEWEB_SSL)
	if(NOT ODC_ASIO_SSL)
		message(FATAL_ERROR "SIMPLEWEB_SSL requires ODC_ASIO_SSL. Enable that option first")
	endif()
endif()

if(USE_SIMPLEWEB_SUBMODULE)
	set(SIMPLEWEB_HOME "${CMAKE_SOURCE_DIR}/Code/submodules/Simple-Web-Server")
	if(NOT EXISTS "${SIMPLEWEB_HOME}/.git")
		execute_process(COMMAND git submodule update --init -- Code/submodules/Simple-Web-Server
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	endif()
else()
	if(DEFINED SIMPLEWEB_HOME)
		set(SIMPLEWEB_HOME ${SIMPLEWEB_HOME} CACHE PATH ${SIMPLEWEB_HOME_INSTRUCTIONS})
	else()
		set(SIMPLEWEB_HOME "/usr/local/src/Simple-Web-Server" CACHE PATH ${SIMPLEWEB_HOME_INSTRUCTIONS})
	endif()
endif()

option(USE_OPENSSL "" SIMPLEWEB_SSL)
option(USE_STANDALONE_ASIO "" ON)
add_library(asio::asio ALIAS ODC)
add_subdirectory(${SIMPLEWEB_HOME} Simple-Web-Server)
#hide third party cmake options as advanced
mark_as_advanced(FORCE Boost_INCLUDE_DIR)
mark_as_advanced(FORCE BUILD_FUZZING)
mark_as_advanced(FORCE BUILD_TESTING)
mark_as_advanced(FORCE USE_OPENSSL)
mark_as_advanced(FORCE USE_STANDALONE_ASIO)

if(SIMPLEWEB_SSL)
	target_compile_definitions(simple-web-server INTERFACE USE_HTTPS)
endif()
