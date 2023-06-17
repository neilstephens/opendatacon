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
project(LuaPort LANGUAGES C CXX)

include(FetchContent)

set(LUA_VERSION "5.4.6")
set(LUA_HASH "SHA256=7d5ea1b9cb6aa0b59ca3dde1c6adcb57ef83a1ba8e5432c0ecd06bf439b3ad88")

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
file(GLOB ${PROJECT_NAME}_SRC ${LUA_SRC}/*.c ${LUA_SRC}/*.h *.cpp *.h *.def)
list(REMOVE_ITEM ${PROJECT_NAME}_SRC ${LUA_SRC}/lua.c ${LUA_SRC}/luac.c)

if(UNIX)
	set(LUA_DEFS "${LUA_DEFS} LUA_USE_POSIX")
endif()

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
target_compile_definitions(${PROJECT_NAME} PRIVATE ${LUA_DEFS})
target_include_directories(${PROJECT_NAME} PRIVATE ${LUA_SRC})
target_link_libraries(${PROJECT_NAME} ODC)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)

install(CODE
"
	set(BUNDLE_DEPS_LIST \${BUNDLE_DEPS_LIST}
		\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_MODULES}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}\${BUNDLE_LIB_POSTFIX}${CMAKE_SHARED_MODULE_SUFFIX}
	)
")
