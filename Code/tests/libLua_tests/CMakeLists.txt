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

set(${PROJECT_NAME}_SRC CatchTestStart.cpp Loading.cpp NetAccessTest.cpp LuaTranslation.cpp)
#the network objects in the Lua hosts other than LuaPort: each host is loaded from its library, as opendatacon does (only those that are built)
set(LUA_HOSTS)
foreach(host LuaTransform LuaLogSink LuaUICommander)
	if(TARGET ${host})
		list(APPEND LUA_HOSTS ${host})
	endif()
endforeach()
list(LENGTH LUA_HOSTS NUM_LUA_HOSTS)
if(NUM_LUA_HOSTS EQUAL 3)
	list(APPEND ${PROJECT_NAME}_SRC NetHostsTest.cpp)
endif()
add_test_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SRC})
#DecideNetAccess() and NetObjectCount()
target_link_libraries(${PROJECT_NAME} LuaWrappers)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Code/Libs/LuaWrappers)
if(NUM_LUA_HOSTS EQUAL 3)
	add_dependencies(${PROJECT_NAME} ${LUA_HOSTS})
	target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Code/Transforms/LuaTransform ${CMAKE_SOURCE_DIR}/Code/Plugins/LuaLogSink
		${CMAKE_SOURCE_DIR}/Code/Plugins/LuaUICommander)
endif()

add_library(LuaTestPlugin MODULE Plugin.cpp Plugin.def)
target_link_libraries(LuaTestPlugin LuaWrappers)
_apply_guard(LuaTestPlugin NO_PCH)
install(TARGETS LuaTestPlugin LIBRARY DESTINATION ${INSTALLDIR_BINS})
set_target_properties(LuaTestPlugin PROPERTIES FOLDER tests)

get_target_property(LUA_INCLUDES ${LUA_LIBNAME} INCLUDE_DIRECTORIES)
if(APPLE)
	set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-undefined,dynamic_lookup")
else()
	string (REGEX REPLACE "-Wl,--no-undefined" "" CMAKE_MODULE_LINKER_FLAGS ${CMAKE_MODULE_LINKER_FLAGS})
endif()

add_library(TestLuaModule MODULE LuaModule.cpp LuaModule.def)
_apply_guard(TestLuaModule NO_PCH)
install(TARGETS TestLuaModule LIBRARY DESTINATION ${INSTALLDIR_BINS})
set_target_properties(TestLuaModule PROPERTIES FOLDER tests)
target_include_directories(TestLuaModule PRIVATE ${LUA_INCLUDES})
if(WIN32)
	target_link_libraries(TestLuaModule ${LUA_LIBNAME})
endif()

#the tests load these at run time
add_dependencies(${PROJECT_NAME} LuaTestPlugin TestLuaModule)
