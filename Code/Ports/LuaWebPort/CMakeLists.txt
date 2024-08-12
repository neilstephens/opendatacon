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
project(LuaWebPort)

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def)

# Locate Simple Web Server
option(USE_SIMPLEWEB_SUBMODULE "Use git submodule to download Simple-Web-Server source" ON)

if(SIMPLEWEB_SSL)
	if(NOT ODC_ASIO_SSL)
		message(FATAL_ERROR "SIMPLEWEB_SSL requires ODC_ASIO_SSL. Enable that option first")
	endif()
	add_definitions(-DUSE_HTTPS)
endif()

if(USE_SIMPLEWEB_SUBMODULE)
	set(SIMPLEWEB_HOME "${CMAKE_SOURCE_DIR}/Code/submodules/Simple-Web-Server")
	if(NOT EXISTS "${SIMPLEWEB_HOME}/.git")
		execute_process(COMMAND git submodule update --init -- Code/submodules/Simple-Web-Server
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	endif()
endif()


add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
target_link_libraries(${PROJECT_NAME} LuaWrappers)
target_link_libraries(${PROJECT_NAME} simple-web-server)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)

install(CODE
"
	set(BUNDLE_DEPS_LIST \${BUNDLE_DEPS_LIST}
		\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_MODULES}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}\${BUNDLE_LIB_POSTFIX}${CMAKE_SHARED_MODULE_SUFFIX}
	)
")
