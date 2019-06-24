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
# Can Cmake find these? Travis std version is 3.6. Appveyor has 3.7

find_package(PythonLibs 3.6 REQUIRED)

include_directories(
	"../include"
	"../JSON"
	${PYTHON_INCLUDE_DIRS}
	"${DNP3_HOME}/include"
	"${ASIO_HOME}/include"
)

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
target_link_libraries(${PROJECT_NAME} ODC ${openpal_libs} ${opendnp3_libs} ${PYTHON_LIBRARIES})

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)
