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
project(Lua C)

include(FetchContent)

#see Code/cmake/version.cmake for where LUA_VERSION is declared

FetchContent_Declare(
	lua
	URL https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz
	URL_HASH ${LUA_HASH}
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
	FetchContent_Populate(lua)
endif()

#TODO: generate this from LUA_VERSION
set(LUA_LIBNAME lua54)
set(LUA_LIBNAME lua54 PARENT_SCOPE)

set(LUA_SRC ${lua_SOURCE_DIR}/src)
file(GLOB ${LUA_LIBNAME}_SRC ${LUA_SRC}/*.c ${LUA_SRC}/*.h)
list(REMOVE_ITEM ${LUA_LIBNAME}_SRC ${LUA_SRC}/lua.c ${LUA_SRC}/luac.c)

set(CMAKE_DEBUG_POSTFIX "")
add_library(${LUA_LIBNAME} SHARED ${${LUA_LIBNAME}_SRC})

if(UNIX)
	target_compile_definitions(${LUA_LIBNAME} PRIVATE LUA_USE_LINUX)
endif()

if(WIN32)
	target_compile_definitions(${LUA_LIBNAME} PRIVATE LUA_USE_WINDOWS)
	target_compile_definitions(${LUA_LIBNAME} PRIVATE LUA_BUILD_AS_DLL)
endif()

if(APPLE)
	target_compile_definitions(${LUA_LIBNAME} PRIVATE LUA_USE_MACOSX)
endif()

target_include_directories(${LUA_LIBNAME} PUBLIC ${LUA_SRC})
target_link_libraries(${LUA_LIBNAME})

install(TARGETS ${LUA_LIBNAME} LIBRARY DESTINATION ${INSTALLDIR_SHARED} ARCHIVE DESTINATION ${INSTALLDIR_LIBS} RUNTIME DESTINATION ${INSTALLDIR_SHARED})
set_target_properties(${LUA_LIBNAME} PROPERTIES FOLDER libs)

install(CODE
"
	set(BUNDLE_DEPS_LIST \${BUNDLE_DEPS_LIST}
		\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_SHARED}/${CMAKE_SHARED_LIBRARY_PREFIX}${LUA_LIBNAME}\${BUNDLE_LIB_POSTFIX}${CMAKE_SHARED_LIBRARY_SUFFIX}
	)
")

