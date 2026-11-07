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
project(LuaWrappers LANGUAGES CXX)

set(lua_lib_filename ${CMAKE_SHARED_LIBRARY_PREFIX}${LUA_LIBNAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
configure_file(DynamicSymbols.cpp.in ${CMAKE_CURRENT_SOURCE_DIR}/DynamicSymbols.cpp)
file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h include/Lua/*.h DynamicSymbols.cpp.in)

add_library(${PROJECT_NAME} SHARED ${${PROJECT_NAME}_SRC})

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_link_libraries(${PROJECT_NAME} ODC ${LUA_LIBNAME} simple-web-server)
if(MSVC)
	#the HTTP objects instantiate the SWS templates (server and client, with and without TLS) in one file
	set_source_files_properties(HttpWrappers.cpp PROPERTIES COMPILE_OPTIONS /bigobj)
endif()

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_SHARED} ARCHIVE DESTINATION ${INSTALLDIR_LIBS} RUNTIME DESTINATION ${INSTALLDIR_SHARED})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER libs)

