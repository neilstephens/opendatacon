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
project(ODC)
cmake_minimum_required(VERSION 2.8)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -DASIO_STANDALONE")
aux_source_directory(. SRC_LIST)
add_library(${PROJECT_NAME} SHARED ${SRC_LIST})

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    if(CMAKE_CXX_COMPILER MATCHES "rpi")
        set(MY_CONFIG_POSTFIX "-rpi-Debug")
    elseif(CMAKE_CXX_COMPILER MATCHES "RHEL65")
        set(MY_CONFIG_POSTFIX "-RHEL-Debug")
    else()
        set(MY_CONFIG_POSTFIX "-Debug")
    endif()
else()
    if(CMAKE_CXX_COMPILER MATCHES "rpi")
        set(MY_CONFIG_POSTFIX "-rpi")
    elseif(CMAKE_CXX_COMPILER MATCHES "RHEL65")
        set(MY_CONFIG_POSTFIX "-RHEL")
    else()
        set(MY_CONFIG_POSTFIX "")
    endif()
endif()

include_directories(
    "../include"
    "../JSON"
	"../../dnp3${MY_CONFIG_POSTFIX}/cpp/libs/asiodnp3/src/"
	"../../dnp3${MY_CONFIG_POSTFIX}/cpp/libs/asiopal/src/"
	"../../dnp3${MY_CONFIG_POSTFIX}/cpp/libs/opendnp3/src/"
	"../../dnp3${MY_CONFIG_POSTFIX}/cpp/libs/openpal/src/"
	"../../asio/include/"
)
