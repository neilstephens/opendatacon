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

# build_zstd.cmake
# Builds zstd as a vendored static dependency for use by librdkafka (Zstandard compression).
# Escape hatch: if zstd_ROOT is already defined and non-empty, the build is skipped.
# After successful build, sets zstd_ROOT (cache) so find_package(zstd) picks it up.
# Note: zstd's CMakeLists.txt lives at Code/submodules/zstd/build/cmake/ (not the repo root).

if(DEFINED zstd_ROOT AND NOT "${zstd_ROOT}" STREQUAL "")
	message(STATUS "zstd_ROOT already set to '${zstd_ROOT}' — skipping vendored zstd build")
	return()
endif()

set(ZSTD_SOURCE "${CMAKE_SOURCE_DIR}/Code/submodules/zstd/build/cmake")
set(ZSTD_BUILD  "${CMAKE_BINARY_DIR}/Code/submodules/zstd")
set(ZSTD_HOME   "${ZSTD_BUILD}/install")

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/Code/submodules/zstd/.git")
	# First try submodule update (works when the path is already registered in the git index)
	execute_process(
		COMMAND git submodule sync -- Code/submodules/zstd
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	)
	execute_process(
		COMMAND git submodule update --init -- Code/submodules/zstd
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		RESULT_VARIABLE EXEC_RESULT
	)
	if(EXEC_RESULT)
		# Fall back to direct shallow clone at the pinned tag.
		message(STATUS "zstd submodule not registered in git index; cloning directly...")
		execute_process(
			COMMAND git clone --depth 1 --branch v1.5.7
				https://github.com/facebook/zstd.git
				"${CMAKE_SOURCE_DIR}/Code/submodules/zstd"
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
			RESULT_VARIABLE EXEC_RESULT
		)
		if(EXEC_RESULT)
			message(FATAL_ERROR "Failed to clone zstd from GitHub. Check network access.")
		endif()
	endif()
endif()

if(NOT EXISTS "${ZSTD_BUILD}")
	file(MAKE_DIRECTORY "${ZSTD_BUILD}")
endif()

if(CMAKE_GENERATOR_PLATFORM)
	set(PLATFORM_OPT "-A${CMAKE_GENERATOR_PLATFORM}")
elseif(CMAKE_VS_PLATFORM_NAME)
	set(PLATFORM_OPT "-A${CMAKE_VS_PLATFORM_NAME}")
else()
	set(PLATFORM_OPT "")
endif()

set(
	ZSTD_CMAKE_OPTS
		-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DBUILD_SHARED_LIBS=OFF
		-DZSTD_BUILD_STATIC=ON
		-DZSTD_BUILD_SHARED=OFF
		-DZSTD_BUILD_PROGRAMS=OFF
		-DZSTD_BUILD_TESTS=OFF
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON
		-DCMAKE_INSTALL_PREFIX=${ZSTD_HOME}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
		-DCMAKE_DEBUG_POSTFIX=${CMAKE_DEBUG_POSTFIX}
	CACHE STRING "cmake options for building zstd submodule"
	FORCE
)

message("Configuring zstd vendor dependency")
execute_process(
	COMMAND ${CMAKE_COMMAND} ${ZSTD_CMAKE_OPTS} -G${CMAKE_GENERATOR} ${PLATFORM_OPT} -S ${ZSTD_SOURCE}
	WORKING_DIRECTORY "${ZSTD_BUILD}"
	OUTPUT_FILE zstd-cmake-output.txt
	RESULT_VARIABLE EXEC_RESULT
)
if(EXEC_RESULT)
	message(FATAL_ERROR "Failed to configure zstd. See ${ZSTD_BUILD}/zstd-cmake-output.txt")
endif()

set(CONFIGS "${CMAKE_BUILD_TYPE}")
if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Rel.*")
	list(APPEND CONFIGS "Release")
endif()
if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Deb.*")
	list(APPEND CONFIGS "Debug")
endif()
foreach(CONF ${CONFIGS})
	message("Building zstd dependency (${CONF})")
	execute_process(
		COMMAND ${CMAKE_COMMAND} --build ${ZSTD_BUILD} --config ${CONF} --parallel 8 --target install
		WORKING_DIRECTORY "${ZSTD_BUILD}"
		OUTPUT_FILE zstd-build-output.txt
		RESULT_VARIABLE EXEC_RESULT
	)
	if(EXEC_RESULT)
		message(FATAL_ERROR "Failed to build zstd. See ${ZSTD_BUILD}/zstd-build-output.txt")
	endif()
endforeach()

set(zstd_ROOT "${ZSTD_HOME}" CACHE PATH "zstd install root (vendored)" FORCE)
message(STATUS "zstd built. zstd_ROOT = ${zstd_ROOT}")
