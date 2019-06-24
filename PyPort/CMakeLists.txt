#	opendatacon
 #
 #	Copyright (c) 2017:
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
project(PyPort)
cmake_minimum_required(VERSION 2.8)
file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def PythonCode/*.py)

# For Appveyor the paths are C:\Python37 for x86 and C:\Python37-x64 for 3.7.3 which is what we are using. Need to specifiy this explicitly?
# Need to apt-get install for travis, for Appveyor we need to set Python_ROOT_DIR so it finds the correct version x86 or x64.

find_package(Python3 COMPONENTS Development)	# The new way of doing things, can pass Python_ROOT_DIR as a hint

message("Found Python Include Dirs ${Python3_INCLUDE_DIRS} ")
message("Found Python Libraries ${Python3_LIBRARIES} ")

include_directories(
	"../include"
	"../JSON"
	${Python3_INCLUDE_DIRS}
	"${DNP3_HOME}/include"
	"${ASIO_HOME}/include"
)

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
target_link_libraries(${PROJECT_NAME} ODC ${openpal_libs} ${opendnp3_libs} ${Python3_LIBRARIES})

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)
