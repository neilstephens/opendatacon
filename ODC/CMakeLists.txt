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
project(ODC)
cmake_minimum_required(VERSION 2.8)
file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def ../include/opendatacon/*.h)

add_library(${PROJECT_NAME} SHARED ${${PROJECT_NAME}_SRC})
add_dependencies(${PROJECT_NAME} version) #make sure version info is up-to-date first
target_include_directories(${PROJECT_NAME} PUBLIC
	"../JSON"
	"../include"
	"${SPDLOG_HOME}/include"
	"${ASIO_HOME}/include")
target_link_libraries(${PROJECT_NAME} JSON ${LIBCXX} ${RT})

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_SHARED} ARCHIVE DESTINATION ${INSTALLDIR_LIBS} RUNTIME DESTINATION ${INSTALLDIR_SHARED})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER libs)

if(PACKAGE_SYSTEM_LIBS)
	install(CODE
	"
		set(BUNDLE_DEPS_LIST \${BUNDLE_DEPS_LIST}
			\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_SHARED}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
		)
	")
endif()
