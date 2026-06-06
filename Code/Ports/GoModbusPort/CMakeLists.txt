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

# Find Go toolchain
set(GO_HOME "/opt/go" CACHE PATH "Directory where Go is installed")
find_program(GO_EXECUTABLE go REQUIRED
  HINTS "${GO_HOME}/bin"
)

set(GO_OUT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}${CMAKE_SHARED_MODULE_SUFFIX}")

get_filename_component(GO_OUT_DIR "${GO_OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${GO_OUT_DIR}")

# Glob all .go sources (for DEPENDS / IDE display)
file(GLOB GO_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.go"
)

# Reconfigure if any .go file is added/removed
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(WIN32)
	# On Windows, LoadLibrary cannot resolve symbols from the calling process
	# the way Linux's dlopen does.  GoModbusPort.dll must declare an explicit
	# import of CAPI.dll so that odc_PublishEvent, odc_Log, etc. are wired up
	# at load time.  We pass CAPI.dll's path via CGO_LDFLAGS so MinGW's linker
	# (used by Go's CGO) reads the DLL's export table and generates the import
	# stubs automatically.  CAPI is listed in DEPENDS to guarantee it is built
	# before the Go build command runs.
	add_custom_command(
	    OUTPUT  ${GO_OUT}
	    COMMAND ${CMAKE_COMMAND} -E env
	            "CGO_LDFLAGS=$<TARGET_FILE:CAPI>"
	        -- ${GO_EXECUTABLE} build
	            -buildmode=c-shared
	            -ldflags "-s -w"
	            -o ${GO_OUT}
	            ${CMAKE_CURRENT_SOURCE_DIR}
	    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	    DEPENDS
	        ${CMAKE_CURRENT_SOURCE_DIR}/go.mod
	        ${GO_SOURCES}
	        CAPI
	    COMMENT "Building GoModbusPort (go build -buildmode=c-shared) [Windows]"
	)
else()
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
endif()

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
