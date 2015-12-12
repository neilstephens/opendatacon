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
project(WebUI)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. WebUI_SRC)
set(WebUI_SRC "${WebUI_SRC}" "WebUI_x64.def")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -DASIO_STANDALONE")

include_directories(
	"../include"
	"../JSON"
	"${MICROHTTPD_HOME}/include"
)

link_directories("${MICROHTTPD_HOME}/${MY_BUILD_CONFIG}/lib"
"${DNP3_HOME}/${MY_BUILD_CONFIG}")
link_libraries(ODC microhttpd)

add_library(${PROJECT_NAME} SHARED ${WebUI_SRC})
