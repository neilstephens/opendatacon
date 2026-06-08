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

# build_libressl.cmake
# Downloads the official LibreSSL release tarball (which includes all generated files)
# and builds it as a vendored static dependency.
# Sets OPENSSL_ROOT_DIR, OPENSSL_USE_STATIC_LIBS, OPENSSL_MSVC_STATIC_RT in cache.
#
# Escape hatch: if OPENSSL_ROOT_DIR is already defined and non-empty, the build is skipped.
# The official release tarball is used instead of the portable git repo because the
# portable GitHub repo requires autogen.sh (autoconf/automake) before cmake works.

if(DEFINED OPENSSL_ROOT_DIR AND NOT "${OPENSSL_ROOT_DIR}" STREQUAL "")
	message(STATUS "OPENSSL_ROOT_DIR already set to '${OPENSSL_ROOT_DIR}' — skipping vendored LibreSSL build")
	return()
endif()

set(LIBRESSL_VERSION "4.3.2")
set(LIBRESSL_TARBALL "libressl-${LIBRESSL_VERSION}.tar.gz")
set(LIBRESSL_URL "https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/${LIBRESSL_TARBALL}")
set(LIBRESSL_SHA256 "edf01aee24c65d69e6a9efcb9d44bcda682ff9d4f3bbbd95e794e1dfa90847b5")

set(LIBRESSL_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/Code/libressl-download")
set(LIBRESSL_SOURCE "${CMAKE_BINARY_DIR}/Code/libressl-src/libressl-${LIBRESSL_VERSION}")
set(LIBRESSL_BUILD  "${CMAKE_BINARY_DIR}/Code/libressl-build")
set(LIBRESSL_HOME   "${LIBRESSL_BUILD}/install")

if(NOT EXISTS "${LIBRESSL_SOURCE}/CMakeLists.txt")
	file(MAKE_DIRECTORY "${LIBRESSL_DOWNLOAD_DIR}")
	set(TARBALL_PATH "${LIBRESSL_DOWNLOAD_DIR}/${LIBRESSL_TARBALL}")
	if(NOT EXISTS "${TARBALL_PATH}")
		message(STATUS "Downloading LibreSSL ${LIBRESSL_VERSION}...")
		file(DOWNLOAD
			"${LIBRESSL_URL}"
			"${TARBALL_PATH}"
			EXPECTED_HASH SHA256=${LIBRESSL_SHA256}
			SHOW_PROGRESS
			STATUS DL_STATUS
		)
		list(GET DL_STATUS 0 DL_RESULT)
		if(DL_RESULT)
			list(GET DL_STATUS 1 DL_MSG)
			message(FATAL_ERROR "Failed to download LibreSSL tarball: ${DL_MSG}")
		endif()
	endif()
	message(STATUS "Extracting LibreSSL ${LIBRESSL_VERSION}...")
	file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/Code/libressl-src")
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E tar xzf "${TARBALL_PATH}"
		WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/Code/libressl-src"
		RESULT_VARIABLE EXEC_RESULT
	)
	if(EXEC_RESULT)
		message(FATAL_ERROR "Failed to extract LibreSSL tarball.")
	endif()
endif()

if(NOT EXISTS "${LIBRESSL_BUILD}")
	file(MAKE_DIRECTORY "${LIBRESSL_BUILD}")
endif()

if(CMAKE_GENERATOR_PLATFORM)
	set(PLATFORM_OPT "-A${CMAKE_GENERATOR_PLATFORM}")
elseif(CMAKE_VS_PLATFORM_NAME)
	set(PLATFORM_OPT "-A${CMAKE_VS_PLATFORM_NAME}")
else()
	set(PLATFORM_OPT "")
endif()

# Translate STATIC_MSVC_RUNTIME to LibreSSL's option (no generator expressions in execute_process)
if(STATIC_MSVC_RUNTIME)
	set(LIBRESSL_MSVC_RT_OPT -DUSE_STATIC_MSVC_RUNTIMES=ON)
else()
	set(LIBRESSL_MSVC_RT_OPT -DUSE_STATIC_MSVC_RUNTIMES=OFF)
endif()

set(
	LIBRESSL_CMAKE_OPTS
		-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DBUILD_SHARED_LIBS=OFF
		-DLIBRESSL_APPS=OFF
		-DLIBRESSL_TESTS=OFF
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON
		-DCMAKE_INSTALL_PREFIX=${LIBRESSL_HOME}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
		${LIBRESSL_MSVC_RT_OPT}
	CACHE STRING "cmake options for building LibreSSL"
	FORCE
)

# On a 64-bit host building a 32-bit target (e.g. i386 Docker container), cmake sets
# CMAKE_SYSTEM_PROCESSOR=x86_64 because uname -m returns the host value.  LibreSSL
# selects ASM files based on CMAKE_SYSTEM_PROCESSOR, so it would pick the x86_64 ASM
# sources (aes-elf-x86_64.S etc.) which use 64-bit-only registers and fail to assemble.
# Force CMAKE_SYSTEM_PROCESSOR=i686 in the inner build so LibreSSL uses its i386 ASM path.
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
	list(APPEND LIBRESSL_CMAKE_OPTS -DCMAKE_SYSTEM_PROCESSOR=i686)
	message(STATUS "i386 cross-build detected: passing -DCMAKE_SYSTEM_PROCESSOR=i686 to LibreSSL")
endif()

message("Configuring LibreSSL ${LIBRESSL_VERSION} vendor dependency")
execute_process(
	COMMAND ${CMAKE_COMMAND} ${LIBRESSL_CMAKE_OPTS} -G${CMAKE_GENERATOR} ${PLATFORM_OPT} -S ${LIBRESSL_SOURCE}
	WORKING_DIRECTORY "${LIBRESSL_BUILD}"
	OUTPUT_FILE libressl-cmake-output.txt
	RESULT_VARIABLE EXEC_RESULT
)
if(EXEC_RESULT)
	message(FATAL_ERROR "Failed to configure LibreSSL. See ${LIBRESSL_BUILD}/libressl-cmake-output.txt")
endif()

set(CONFIGS "${CMAKE_BUILD_TYPE}")
if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Rel.*")
	list(APPEND CONFIGS "Release")
endif()
if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Deb.*")
	list(APPEND CONFIGS "Debug")
endif()
foreach(CONF ${CONFIGS})
	message("Building LibreSSL dependency (${CONF})")
	execute_process(
		COMMAND ${CMAKE_COMMAND} --build ${LIBRESSL_BUILD} --config ${CONF} --parallel 8 --target install
		WORKING_DIRECTORY "${LIBRESSL_BUILD}"
		OUTPUT_FILE libressl-build-output.txt
		RESULT_VARIABLE EXEC_RESULT
	)
	if(EXEC_RESULT)
		message(FATAL_ERROR "Failed to build LibreSSL. See ${LIBRESSL_BUILD}/libressl-build-output.txt")
	endif()
endforeach()

# Expose install root and static-link settings to every consumer downstream
set(OPENSSL_ROOT_DIR "${LIBRESSL_HOME}" CACHE PATH "LibreSSL install root (vendored)" FORCE)
set(OPENSSL_USE_STATIC_LIBS TRUE CACHE BOOL "Use static OpenSSL/LibreSSL libraries" FORCE)
set(OPENSSL_MSVC_STATIC_RT TRUE CACHE BOOL "Use static MSVC runtime for OpenSSL" FORCE)
message(STATUS "LibreSSL ${LIBRESSL_VERSION} built. OPENSSL_ROOT_DIR = ${OPENSSL_ROOT_DIR}")
