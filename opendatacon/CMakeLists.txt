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
project(opendatacon)
file(GLOB SRC_LIST *.cpp *.h)

if(WIN32)
	file(GLOB DAEMON_SRC_LIST WinDaemon/*.cpp WinDaemon/*.h)
else()
	file(GLOB DAEMON_SRC_LIST POSIXDaemon/*.cpp POSIXDaemon/*.h)
endif()

option(USE_TCLAP_SUBMODULE "Use git submodule to download tclap header library" ON)

set(TCLAP_HOME_INSTRUCTIONS "Choose the location of tclap header library: where it was installed - there should be an include directory within")
if(USE_TCLAP_SUBMODULE)
	set(TCLAP_HOME "${CMAKE_SOURCE_DIR}/submodules/tclap" CACHE PATH ${TCLAP_HOME_INSTRUCTIONS} FORCE)
	if(NOT EXISTS "${TCLAP_HOME}/.git")
		execute_process(COMMAND git submodule update --init -- submodules/tclap
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	endif()
else()
	if(DEFINED TCLAP_HOME)
		set(TCLAP_HOME ${TCLAP_HOME} CACHE PATH ${TCLAP_HOME_INSTRUCTIONS})
	else()
		set(TCLAP_HOME "/usr" CACHE PATH ${TCLAP_HOME_INSTRUCTIONS})
	endif()
endif()

add_executable(${PROJECT_NAME} ${SRC_LIST} ${DAEMON_SRC_LIST})
target_include_directories(${PROJECT_NAME} PRIVATE "${TCLAP_HOME}/include")
target_link_libraries(${PROJECT_NAME} ODC ${DL})

install(DIRECTORY "../include/opendatacon" DESTINATION ${INSTALLDIR_INCLUDES})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${INSTALLDIR_BINS})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER bins)

if(PACKAGE_SYSTEM_LIBS)
	install(CODE
	"
		set(BUNDLE_DEPS_LIST \${BUNDLE_DEPS_LIST}
			\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_BINS}/${PROJECT_NAME}
		)
	")
endif()
