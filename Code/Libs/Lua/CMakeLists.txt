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
project(Lua LANGUAGES C CXX)
cmake_minimum_required(VERSION 3.14)

set(DOWNLOAD_EXTRACT_TIMESTAMP true)
include(FetchContent)

#see Code/cmake/version.cmake for where LUA_VERSION is declared

FetchContent_Declare(
	lua
	URL https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz
	URL_HASH ${LUA_HASH}
)

FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
	FetchContent_Populate(lua)
endif()

set(LUA_SRC ${lua_SOURCE_DIR}/src)
file(GLOB ${PROJECT_NAME}_SRC_C ${LUA_SRC}/*.c ${LUA_SRC}/*.h)
list(REMOVE_ITEM ${PROJECT_NAME}_SRC_C ${LUA_SRC}/lua.c ${LUA_SRC}/luac.c)
file(GLOB ${PROJECT_NAME}_SRC_CXX *.cpp *.h include/Lua/*.h)

add_library(${PROJECT_NAME} SHARED ${${PROJECT_NAME}_SRC_C} ${${PROJECT_NAME}_SRC_CXX})

foreach(SRC_FILENAME ${${PROJECT_NAME}_SRC_C})
	set_property(SOURCE SRC_FILENAME PROPERTY LANGUAGE C)
	if(SRC_FILENAME MATCHES ".h")
		set_property(SOURCE SRC_FILENAME PROPERTY HEADER_FILE_ONLY TRUE)
	endif()
endforeach()

foreach(SRC_FILENAME ${${PROJECT_NAME}_SRC_CXX})
	set_property(SOURCE SRC_FILENAME PROPERTY LANGUAGE C++)
	if(SRC_FILENAME MATCHES ".h")
		set_property(SOURCE SRC_FILENAME PROPERTY HEADER_FILE_ONLY TRUE)
	endif()
endforeach()

if(UNIX)
	target_compile_definitions(${PROJECT_NAME} PRIVATE LUA_USE_LINUX)
endif()

if(WIN32)
	target_compile_definitions(${PROJECT_NAME} PRIVATE LUA_USE_WINDOWS)
	target_compile_definitions(${PROJECT_NAME} PRIVATE LUA_BUILD_AS_DLL)
	#FIXME: throw an error if LUA_VERSION not 5.4.x so the below links can be updated or make the '54' dynamic
	#copy Lua.dll -> lua54.dll
	add_custom_target(lua_symlink_target ALL
		COMMAND ${CMAKE_COMMAND} -E create_symlink "Lua${CMAKE_DEBUG_POSTFIX}.dll" "lua54.dll"
		COMMAND ${CMAKE_COMMAND} -E create_symlink "Lua${CMAKE_DEBUG_POSTFIX}.lib" "lua54.lib"
	)
	add_dependencies(lua_symlink_target ${PROJECT_NAME})
	#import lua54.dll as a global import library target (test lua module links against it)
	add_library(lua_proxy_target SHARED IMPORTED GLOBAL)
	set_property(TARGET lua_proxy_target PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lua54.dll")
	set_property(TARGET lua_proxy_target PROPERTY IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/lua54.lib")
	add_dependencies(lua_proxy_target lua_symlink_target)
	install(FILES "lua54.dll" DESTINATION ${INSTALLDIR_SHARED})
endif()

if(APPLE)
	target_compile_definitions(${PROJECT_NAME} PRIVATE LUA_USE_MACOSX)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC ${LUA_SRC} include)
target_link_libraries(${PROJECT_NAME} ODC)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_SHARED} ARCHIVE DESTINATION ${INSTALLDIR_LIBS} RUNTIME DESTINATION ${INSTALLDIR_SHARED})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER libs)

install(CODE
"
	set(BUNDLE_DEPS_LIST \${BUNDLE_DEPS_LIST}
		\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_SHARED}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}\${BUNDLE_LIB_POSTFIX}${CMAKE_SHARED_LIBRARY_SUFFIX}
	)
")

