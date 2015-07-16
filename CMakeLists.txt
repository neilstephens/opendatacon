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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -DASIO_STANDALONE")

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    if(CMAKE_CXX_COMPILER MATCHES "rpi")
        set(MY_CONFIG_POSTFIX "-rpi-Debug")
        set(MY_BUILD_CONFIG "Debug-rpi")
    elseif(CMAKE_CXX_COMPILER MATCHES "RHEL65")
        set(MY_CONFIG_POSTFIX "-RHEL-Debug")
        set(MY_BUILD_CONFIG "Debug-RHEL65")
    else()
        set(MY_CONFIG_POSTFIX "-Debug")
        set(MY_BUILD_CONFIG "Debug")
    endif()
else()
    if(CMAKE_CXX_COMPILER MATCHES "rpi")
        set(MY_CONFIG_POSTFIX "-rpi")
        set(MY_BUILD_CONFIG "Release-rpi")
    elseif(CMAKE_CXX_COMPILER MATCHES "RHEL65")
        set(MY_CONFIG_POSTFIX "-RHEL")
		set(MY_BUILD_CONFIG "Release-RHEL65")
    else()
        set(MY_CONFIG_POSTFIX "")
        set(MY_BUILD_CONFIG "Release")
    endif()
endif()

add_subdirectory(JSON)
add_subdirectory(ODC)
add_subdirectory(DNP3Port)
add_subdirectory(JSONPort)
add_subdirectory(WebUI)
add_subdirectory(tests)

include_directories(
	"include"
	"JSON"
	"${DNP3_HOME}/dnp3${MY_CONFIG_POSTFIX}/cpp/libs/asiodnp3/src/"
	"${DNP3_HOME}/dnp3${MY_CONFIG_POSTFIX}/cpp/libs/asiopal/src/"
	"${DNP3_HOME}/dnp3${MY_CONFIG_POSTFIX}/cpp/libs/opendnp3/src/"
	"${DNP3_HOME}/dnp3${MY_CONFIG_POSTFIX}/cpp/libs/openpal/src/"
	"${ASIO_HOME}/asio/include/"
	"${TCLAP_HOME}/tclap/include/"
)
link_directories("${DNP3_HOME}/dnp3${MY_CONFIG_POSTFIX}/.libs")
link_libraries(ODC JSON asiopal openpal asiodnp3 opendnp3 pthread dl)
add_executable(${PROJECT_NAME} ${SRC_LIST})
