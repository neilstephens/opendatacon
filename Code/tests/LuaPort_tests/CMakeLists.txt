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
project(LuaPort_tests)

set(LUA_FILE "${CMAKE_SOURCE_DIR}/ExampleConfig/SelfDocumentingLuaPort/LuaPortDoco.lua")
set(HELLO_FILE "${CMAKE_SOURCE_DIR}/ExampleConfig/SelfDocumentingLuaPort/my_modules/hello.lua")

get_filename_component(NOEXT ${LUA_FILE} NAME_WE)
set(LUA_VAR_NAME ${NOEXT}_lua)
set(HELLO_VAR_NAME ${NOEXT}_hello)

message("Reading ${LUA_FILE} into ${LUA_VAR_NAME}")
file(READ ${LUA_FILE} ${LUA_VAR_NAME} PARENT_SCOPE)

message("Reading ${HELLO_FILE} into ${HELLO_VAR_NAME}")
file(READ ${HELLO_FILE} ${HELLO_VAR_NAME} PARENT_SCOPE)

configure_file(ConfStrings.cpp.in ${CMAKE_CURRENT_SOURCE_DIR}/ConfStrings.cpp)

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h ../PortLoader.cpp ../PortLoader.h ConfStrings.cpp.in)

add_test_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SRC})
