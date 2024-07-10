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
project(KafkaPort)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/Code/submodules/librdkafka/.git")
	execute_process(COMMAND git submodule update --init -- Code/submodules/librdkafka
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

#TODO: consider building and installing librdkafka separately instead of add_subdirectory
#	- would allow for more control over the build
#	- could avoid linking libs that are already compiled into ODC (openSSL etc.)
#	- would be aligned with the way dnp3 is built
set(RDKAFKA_BUILD_STATIC ON CACHE BOOL "")
set(RDKAFKA_BUILD_EXAMPLES OFF CACHE BOOL "")
set(RDKAFKA_BUILD_TESTS OFF CACHE BOOL "")
add_subdirectory(${CMAKE_SOURCE_DIR}/Code/submodules/librdkafka ${CMAKE_BINARY_DIR}/Code/submodules/librdkafka SYSTEM)
# copy the headers - only needed because add_subdirectory is used. Otherwise the target install would take care of this
configure_file ("${CMAKE_SOURCE_DIR}/Code/submodules/librdkafka/src/rdkafka.h"
				"librdkafka/rdkafka.h" COPYONLY)
configure_file ("${CMAKE_SOURCE_DIR}/Code/submodules/librdkafka/src/rdkafka_mock.h"
				"librdkafka/rdkafka_mock.h" COPYONLY)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/Code/submodules/modern-cpp-kafka/.git")
	execute_process(COMMAND git submodule update --init -- Code/submodules/modern-cpp-kafka
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def)

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
target_link_libraries(${PROJECT_NAME} ODC rdkafka)
target_include_directories(${PROJECT_NAME} PRIVATE SYSTEM ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}/Code/submodules/modern-cpp-kafka/include)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)

install(CODE
"
	set(BUNDLE_DEPS_LIST \${BUNDLE_DEPS_LIST}
		\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_MODULES}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}\${BUNDLE_LIB_POSTFIX}${CMAKE_SHARED_MODULE_SUFFIX}
	)
")
