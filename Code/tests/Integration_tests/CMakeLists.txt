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
project(Integration_tests)

set(CONF_PATH "${CMAKE_SOURCE_DIR}/ExampleConfig/ConfigReload")
file(GLOB CONF_FILES ${CONF_PATH}/*.conf)
foreach(CONF_FILE ${CONF_FILES})
	get_filename_component(NOEXT ${CONF_FILE} NAME_WE)
	set(VAR_NAME ${NOEXT}_conf)
	message("Reading ${CONF_FILE} into ${VAR_NAME}")
	file(READ ${CONF_FILE} ${VAR_NAME} PARENT_SCOPE)
endforeach()

set(CONF_FILE "${CMAKE_SOURCE_DIR}/ExampleConfig/FileTransferTestEtoE/transfer.conf")
get_filename_component(NOEXT ${CONF_FILE} NAME_WE)
set(VAR_NAME ${NOEXT}_conf)
message("Reading ${CONF_FILE} into ${VAR_NAME}")
file(READ ${CONF_FILE} ${VAR_NAME} PARENT_SCOPE)

configure_file(ConfStrings.cpp.in ${CMAKE_CURRENT_SOURCE_DIR}/ConfStrings.cpp)

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h ../../opendatacon/DataConcentrator.cpp ../../opendatacon/DataConnector.cpp ConfStrings.cpp.in)
add_definitions(-DMAIN_EXE)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SRC} $<TARGET_OBJECTS:VersionStrings>)
target_link_libraries(${PROJECT_NAME} LuaUICommander ${DL})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${INSTALLDIR_BINS})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER tests)
add_test(NAME CTEST_${PROJECT_NAME} COMMAND ${PROJECT_NAME})
