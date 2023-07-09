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
project(libLua_tests)

set(${PROJECT_NAME}_SRC CatchTestStart.cpp Loading.cpp)
add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SRC})
target_link_libraries(${PROJECT_NAME} ODC ${DL})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${INSTALLDIR_BINS})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER tests)
add_test(NAME CTEST_${PROJECT_NAME} COMMAND ${PROJECT_NAME})

add_library(LuaTestPlugin MODULE Plugin.cpp)
target_link_libraries(LuaTestPlugin Lua)
install(TARGETS LuaTestPlugin LIBRARY DESTINATION ${INSTALLDIR_BINS})
set_target_properties(LuaTestPlugin PROPERTIES FOLDER tests)

get_target_property(LUA_INCLUDES Lua INCLUDE_DIRECTORIES)
string (REGEX REPLACE "-Wl,--no-undefined" "" CMAKE_MODULE_LINKER_FLAGS ${CMAKE_MODULE_LINKER_FLAGS})

add_library(TestLuaModule MODULE LuaModule.cpp)
install(TARGETS TestLuaModule LIBRARY DESTINATION ${INSTALLDIR_BINS})
set_target_properties(TestLuaModule PROPERTIES FOLDER tests)
target_include_directories(TestLuaModule PRIVATE ${LUA_INCLUDES})

