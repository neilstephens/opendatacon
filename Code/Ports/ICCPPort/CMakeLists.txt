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
project(ICCPPort)

# The port's sources are compiled once into an object library. The ICCPPort module adds the plugin entry points (main.cpp) to them, and the
# unit tests (Code/tests/ICCPPort_tests) add themselves to the same objects in their own executable, because they need the port's internals.
# The module therefore never contains test code, whether or not TESTS is on.
file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h)
list(REMOVE_ITEM ${PROJECT_NAME}_SRC ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

add_library(${PROJECT_NAME}_objects OBJECT ${${PROJECT_NAME}_SRC})
target_link_libraries(${PROJECT_NAME}_objects PRIVATE ODC)
set_target_properties(${PROJECT_NAME}_objects PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER ports)
_apply_guard(${PROJECT_NAME}_objects)

add_port_library(${PROJECT_NAME} main.cpp ICCPPort.def $<TARGET_OBJECTS:${PROJECT_NAME}_objects>)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# secure ICCP (TLS, IEC 62351-3) needs OpenSSL: libODC links it when ODC_ASIO_SSL is ON. The module, the objects and the tests must agree on it.
if(ODC_ASIO_SSL)
	target_compile_definitions(${PROJECT_NAME} PRIVATE ODC_ASIO_SSL)
	target_compile_definitions(${PROJECT_NAME}_objects PRIVATE ODC_ASIO_SSL)
endif()
