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
project(KafkaPort CXX)

#TODO: make using the submodules optional (to use local copies of dependencies),
#	similar to other ports that have optional submodule deps
#		with USE_SUBMODULE option and DEP_HOME

#script to download, build, install, and import librdkafka
include(build_librdkafka.cmake)
if(WIN32 AND ODC_ASIO_SSL)
		# We have to tell the linker to ignore default libs that librdkafka hard coded with pragmas!
		target_link_options(${PROJECT_NAME} PRIVATE "/NODEFAULTLIB:libssl.lib" "/NODEFAULTLIB:libcrypto.lib")
endif()

#and this is the modern C++ wrapper for librdkafka
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/Code/submodules/modern-cpp-kafka/.git")
	execute_process(COMMAND git submodule update --init -- Code/submodules/modern-cpp-kafka
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

#and the CBOR library (it's a whole json lib with CBOR extension, but the best CBOR interface I've found)
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/Code/submodules/jsoncons/.git")
	execute_process(COMMAND git submodule update --init -- Code/submodules/jsoncons
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

#this is the actual port
file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def)
add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})

target_link_libraries(${PROJECT_NAME} ODC RdKafka::rdkafka)
target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_SOURCE_DIR}/Code/submodules/modern-cpp-kafka/include
	${CMAKE_SOURCE_DIR}/Code/submodules/jsoncons/include)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)

install(CODE
"
	set(BUNDLE_DEPS_LIST \${BUNDLE_DEPS_LIST}
		\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_MODULES}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}\${BUNDLE_LIB_POSTFIX}${CMAKE_SHARED_MODULE_SUFFIX}
	)
")
