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
project(ICCPPort_tests)

# White box unit tests: they need the port's internals, so they are compiled together with the port's own objects (the ICCPPort_objects library,
# see Code/Ports/ICCPPort/CMakeLists.txt) into this executable, with their own Catch main (TestICCPPort.cpp). The ICCPPort module carries no test code.
file(GLOB ${PROJECT_NAME}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*Test.cpp)
list(APPEND ${PROJECT_NAME}_SRC TestICCPPort.cpp)

add_test_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SRC} $<TARGET_OBJECTS:ICCPPort_objects>)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Code/Ports/ICCPPort ${CMAKE_CURRENT_SOURCE_DIR})
# TlsTestPki.h (the throw-away PKI of the TLS tests) is shared with the ODC library's tests, which own the TLS filter
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Code/tests/ODC_tests)
if(ODC_ASIO_SSL)
	target_compile_definitions(${PROJECT_NAME} PRIVATE ODC_ASIO_SSL)
endif()
