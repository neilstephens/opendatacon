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
project(LuaPort_tests)

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h ../PortLoader.cpp ../PortLoader.h)

if("${CMAKE_CONFIGURATION_TYPES}" STREQUAL "")
	configure_file(test.lua ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test.lua COPYONLY )
else()
	foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
		string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
		configure_file(test.lua ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}}/test.lua COPYONLY )
	endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )
endif()

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SRC})
target_link_libraries(${PROJECT_NAME} ODC ${DL})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${INSTALLDIR_BINS})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER tests)
add_test(NAME CTEST_${PROJECT_NAME} COMMAND ${PROJECT_NAME})
