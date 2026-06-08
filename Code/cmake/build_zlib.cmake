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

# build_zlib.cmake
# Builds zlib as a vendored static dependency for use by librdkafka (GZIP compression).
# Escape hatch: if ZLIB_ROOT is already defined and non-empty, the build is skipped.
# After successful build, sets ZLIB_ROOT (cache) so find_package(ZLIB) picks it up.

if(DEFINED ZLIB_ROOT AND NOT "${ZLIB_ROOT}" STREQUAL "")
	message(STATUS "ZLIB_ROOT already set to '${ZLIB_ROOT}' — skipping vendored zlib build")
	return()
endif()

if(APPLE)
	# On macOS, embedding a static zlib in a MODULE bundle causes dlopen to fail because
	# macOS dyld4 (macOS 12+) treats duplicate strong-symbol definitions as a load error:
	# system libz.dylib is already in every process (pulled by libSystem/CoreFoundation),
	# and adding a second copy via a static libz.a in the bundle triggers the conflict.
	# Skip the vendored build and let build_librdkafka.cmake fall back to the system zlib.
	message(STATUS "macOS: skipping vendored zlib build — system libz.dylib will be used")
	return()
endif()

set(ZLIB_SOURCE "${CMAKE_SOURCE_DIR}/Code/submodules/zlib")
set(ZLIB_BUILD  "${CMAKE_BINARY_DIR}/Code/submodules/zlib")
set(ZLIB_HOME   "${ZLIB_BUILD}/install")

if(NOT EXISTS "${ZLIB_SOURCE}/.git")
	# First try submodule update (works when the path is already registered in the git index)
	execute_process(
		COMMAND git submodule sync -- Code/submodules/zlib
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	)
	execute_process(
		COMMAND git submodule update --init -- Code/submodules/zlib
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		RESULT_VARIABLE EXEC_RESULT
	)
	if(EXEC_RESULT)
		# Fall back to direct shallow clone at the pinned tag.
		message(STATUS "zlib submodule not registered in git index; cloning directly...")
		execute_process(
			COMMAND git clone --depth 1 --branch v1.3.2
				https://github.com/madler/zlib.git "${ZLIB_SOURCE}"
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
			RESULT_VARIABLE EXEC_RESULT
		)
		if(EXEC_RESULT)
			message(FATAL_ERROR "Failed to clone zlib from GitHub. Check network access.")
		endif()
	endif()
endif()

if(NOT EXISTS "${ZLIB_BUILD}")
	file(MAKE_DIRECTORY "${ZLIB_BUILD}")
endif()

if(CMAKE_GENERATOR_PLATFORM)
	set(PLATFORM_OPT "-A${CMAKE_GENERATOR_PLATFORM}")
elseif(CMAKE_VS_PLATFORM_NAME)
	set(PLATFORM_OPT "-A${CMAKE_VS_PLATFORM_NAME}")
else()
	set(PLATFORM_OPT "")
endif()

set(
	ZLIB_CMAKE_OPTS
		-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DBUILD_SHARED_LIBS=OFF
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON
		-DCMAKE_INSTALL_PREFIX=${ZLIB_HOME}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
		-DCMAKE_DEBUG_POSTFIX=${CMAKE_DEBUG_POSTFIX}
	CACHE STRING "cmake options for building zlib submodule"
	FORCE
)

message("Configuring zlib vendor dependency")
execute_process(
	COMMAND ${CMAKE_COMMAND} ${ZLIB_CMAKE_OPTS} -G${CMAKE_GENERATOR} ${PLATFORM_OPT} -S ${ZLIB_SOURCE}
	WORKING_DIRECTORY "${ZLIB_BUILD}"
	OUTPUT_FILE zlib-cmake-output.txt
	RESULT_VARIABLE EXEC_RESULT
)
if(EXEC_RESULT)
	message(FATAL_ERROR "Failed to configure zlib. See ${ZLIB_BUILD}/zlib-cmake-output.txt")
endif()

set(CONFIGS "${CMAKE_BUILD_TYPE}")
if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Rel.*")
	list(APPEND CONFIGS "Release")
endif()
if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Deb.*")
	list(APPEND CONFIGS "Debug")
endif()
foreach(CONF ${CONFIGS})
	message("Building zlib dependency (${CONF})")
	execute_process(
		COMMAND ${CMAKE_COMMAND} --build ${ZLIB_BUILD} --config ${CONF} --parallel 8 --target install
		WORKING_DIRECTORY "${ZLIB_BUILD}"
		OUTPUT_FILE zlib-build-output.txt
		RESULT_VARIABLE EXEC_RESULT
	)
	if(EXEC_RESULT)
		message(FATAL_ERROR "Failed to build zlib. See ${ZLIB_BUILD}/zlib-build-output.txt")
	endif()
endforeach()

set(ZLIB_ROOT "${ZLIB_HOME}" CACHE PATH "zlib install root (vendored)" FORCE)
message(STATUS "zlib built. ZLIB_ROOT = ${ZLIB_ROOT}")
