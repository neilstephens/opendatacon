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
file(GLOB SRC_LIST *.cpp *.h)
include_directories(
	"../include"
	"../JSON"
	"${DNP3_HOME}/include"
	"${ASIO_HOME}/include"
	"${TCLAP_HOME}/include"
)
link_directories("${DNP3_HOME}/lib")

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} ODC JSON asiopal openpal asiodnp3 opendnp3 ${PTHREAD} ${DL})

install(DIRECTORY "include/opendatacon" DESTINATION "include")
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "bin")
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER bins)
