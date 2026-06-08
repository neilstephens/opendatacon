#	opendatacon
#
#	Copyright (c) 2025:
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
project(GoModbusPort)

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "GoModbusPort cannot be built on 32-bit Windows: Go c-shared buildmode does not support windows/386")
endif()

# Find Go toolchain
set(GO_HOME "/opt/go" CACHE PATH "Directory where Go is installed")
find_program(GO_EXECUTABLE go REQUIRED
  HINTS "${GO_HOME}/bin"
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_CONFIGURATION_TYPES STREQUAL "Debug")
	set(_go_postfix "${CMAKE_DEBUG_POSTFIX}")
else()
	set(_go_postfix "")
endif()

set(GO_OUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}${_go_postfix}${CMAKE_SHARED_MODULE_SUFFIX}")

get_filename_component(GO_OUT_DIR "${GO_OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${GO_OUT_DIR}")

# Glob all .go sources (for DEPENDS / IDE display)
file(GLOB GO_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.go"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# Reconfigure if any .go file is added/removed
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_custom_command(
    OUTPUT  ${GO_OUT}
    COMMAND ${GO_EXECUTABLE} build
            -buildmode=c-shared
            -ldflags "-s -w"
            -o ${GO_OUT}
            ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/go.mod
        ${GO_SOURCES}
    COMMENT "Building GoModbusPort (go build -buildmode=c-shared)"
)

# ALL target drives the custom command
add_custom_target(${PROJECT_NAME} ALL DEPENDS ${GO_OUT}
    SOURCES
        ${GO_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/go.mod
)

# Imported SHARED target so dependents (GoModbusPort_tests) can declare a
# proper CMake dependency on the .so without linking it.
add_library(${PROJECT_NAME}_lib SHARED IMPORTED GLOBAL)
set_property(TARGET ${PROJECT_NAME}_lib PROPERTY IMPORTED_LOCATION ${GO_OUT})
add_dependencies(${PROJECT_NAME}_lib ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME}_lib PROPERTIES FOLDER ports)

install(FILES ${GO_OUT} DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)
