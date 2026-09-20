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
project(DNP3Port_tests)

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h ../PortLoader.cpp ../PortLoader.h ../ManInTheMiddle.cpp ../ManInTheMiddle.h ../UDPMITM.cpp ../UDPMITM.h)

add_test_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE mock)

# Shares real loopback ports 20000/20001 with Integration_tests
# (ExampleConfig/ConfigReload/DNP3_A.conf, opendatacon.conf). Lock name must
# match the one set in Integration_tests/CMakeLists.txt.
set_tests_properties(CTEST_${PROJECT_NAME} PROPERTIES RESOURCE_LOCK dnp3_loopback_ports_20000_20001)
