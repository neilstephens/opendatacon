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

if(NOT ODC_ASIO_SSL)
	message(WARNING "KafkaPort requires ODC_ASIO_SSL to be enabled for librdkafka SSL support.")
endif()

# zlib and zstd are built and their cmake targets created in the top-level CMakeLists.txt
# (inside if(KAFKAPORT), before add_subdirectory(Code/Libs/ODC)).  They are also linked
# PRIVATELY into libODC there.  ZLIB_ROOT / zstd_ROOT cache variables are already set.

# Build cmake-opts fragments for zlib and zstd used in the inner librdkafka configure.
if(DEFINED ZLIB_ROOT AND NOT "${ZLIB_ROOT}" STREQUAL "")
	set(RDKAFKA_ZLIB_OPTS -DWITH_ZLIB=ON -DZLIB_ROOT=${ZLIB_ROOT})
else()
	# No vendored zlib (e.g. macOS): still enable zlib compression but let librdkafka's
	# cmake find the system zlib itself (no ZLIB_ROOT restriction).
	set(RDKAFKA_ZLIB_OPTS -DWITH_ZLIB=ON)
endif()
if(DEFINED zstd_ROOT AND NOT "${zstd_ROOT}" STREQUAL "")
	set(RDKAFKA_ZSTD_OPTS -DWITH_ZSTD=ON "-DCMAKE_PREFIX_PATH=${zstd_ROOT};${ZLIB_ROOT}")
else()
	# No vendored zstd (e.g. macOS): disable zstd in librdkafka to avoid unresolved deps.
	set(RDKAFKA_ZSTD_OPTS -DWITH_ZSTD=OFF)
endif()

if(MSVC)
	set(NOWARN_C_FLAGS "${CMAKE_C_FLAGS} /W0") #don't want warnings from external librdkafka code
	set(MSVC_OPTS
		"-DOPENSSL_MSVC_STATIC_RT=${OPENSSL_MSVC_STATIC_RT}"
		"-DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}"
	)
else()
	set(NOWARN_C_FLAGS "${CMAKE_C_FLAGS} -w") #don't want warnings from external librdkafka code
endif()

set(RDKAFKA_SOURCE_ORIG "${CMAKE_SOURCE_DIR}/Code/submodules/librdkafka")
set(RDKAFKA_BUILD "${CMAKE_BINARY_DIR}/Code/submodules/librdkafka")
# Patched copy of the submodule sources lives in the build tree so the
# submodule working tree is never modified (keeps git submodule status clean).
set(RDKAFKA_SRC_BUILD "${RDKAFKA_BUILD}/src")
mark_as_advanced(FORCE RDKAFKA_SOURCE_ORIG)
mark_as_advanced(FORCE RDKAFKA_BUILD)
set(RDKAFKA_HOME "${RDKAFKA_BUILD}/install")
set(
	RDKAFKA_CMAKE_OPTS
		-DCMAKE_C_FLAGS=${NOWARN_C_FLAGS}
		-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DRDKAFKA_BUILD_STATIC=ON
		-DENABLE_LZ4_EXT=OFF
		-DRDKAFKA_BUILD_EXAMPLES=OFF
		-DRDKAFKA_BUILD_TESTS=OFF
		-DOPENSSL_USE_STATIC_LIBS=${OPENSSL_USE_STATIC_LIBS}
		${MSVC_OPTS}
		-DWITH_SSL=${ODC_ASIO_SSL}
		-DWITH_CURL=OFF
		-DWITH_SASL_CYRUS=OFF
		${RDKAFKA_ZLIB_OPTS}
		${RDKAFKA_ZSTD_OPTS}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
		-DCMAKE_INSTALL_PREFIX=${RDKAFKA_HOME}/
		-DCMAKE_DEBUG_POSTFIX=${CMAKE_DEBUG_POSTFIX}
		-DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5
	CACHE STRING "cmake options to use when building librdkafka submodule"
	FORCE
)
if(NOT EXISTS "${RDKAFKA_SOURCE_ORIG}/.git")
	execute_process(COMMAND git submodule update --init -- Code/submodules/librdkafka
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
if(NOT EXISTS "${RDKAFKA_BUILD}")
	file(MAKE_DIRECTORY "${RDKAFKA_BUILD}")
endif()

# ── Copy submodule sources to build tree (once) then apply patches ──────────
# Patching the build-tree copy keeps the submodule working tree clean.
if(NOT EXISTS "${RDKAFKA_SRC_BUILD}/CMakeLists.txt")
	message(STATUS "Copying librdkafka sources to build tree: ${RDKAFKA_SRC_BUILD}")
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${RDKAFKA_SOURCE_ORIG}" "${RDKAFKA_SRC_BUILD}"
		RESULT_VARIABLE COPY_RESULT
	)
	if(COPY_RESULT)
		message(FATAL_ERROR "Failed to copy librdkafka sources to build tree.")
	endif()
	# Remove stale cmake cache so the configure step picks up the new source path.
	file(REMOVE "${RDKAFKA_BUILD}/CMakeCache.txt")
endif()

# Patch 1: rdkafka_conf.h — exclude LibreSSL from the WITH_SSL_ENGINE block.
# LibreSSL 3.5+ dropped the ENGINE API; this prevents a compile error when
# OPENSSL_VERSION_NUMBER satisfies the guard but the ENGINE header is absent.
set(_patch_file "${RDKAFKA_SRC_BUILD}/src/rdkafka_conf.h")
file(READ "${_patch_file}" _content)
string(FIND "${_content}" "LIBRESSL_VERSION_NUMBER" _already_patched)
if(_already_patched EQUAL -1)
	message(STATUS "Patching librdkafka: rdkafka_conf.h LibreSSL ENGINE guard")
	string(REPLACE
		"!defined(OPENSSL_IS_BORINGSSL)"
		"!defined(OPENSSL_IS_BORINGSSL) && !defined(LIBRESSL_VERSION_NUMBER)"
		_content "${_content}"
	)
	file(WRITE "${_patch_file}" "${_content}")
else()
	message(STATUS "librdkafka rdkafka_conf.h: LibreSSL ENGINE guard already present")
endif()
unset(_patch_file)
unset(_content)
unset(_already_patched)

# Patch 2: librdkafka CMakeLists.txt — make WITH_SASL_CYRUS an independently
# settable option so that PLAIN/SCRAM (OpenSSL-only, builtin) can be enabled
# without Kerberos/GSSAPI (libsasl2 + MIT Kerberos, not vendored).
set(_patch_file "${RDKAFKA_SRC_BUILD}/CMakeLists.txt")
file(READ "${_patch_file}" _content)
string(FIND "${_content}" "option(WITH_SASL_CYRUS" _already_patched)
if(_already_patched EQUAL -1)
	message(STATUS "Patching librdkafka: CMakeLists.txt WITH_SASL_CYRUS option")
	string(REPLACE
		"  if(NOT WIN32)\n    set(WITH_SASL_CYRUS ON)\n    list(APPEND BUILT_WITH \"SASL_CYRUS\")\n  endif()"
		"  if(NOT WIN32)\n    option(WITH_SASL_CYRUS \"With SASL Cyrus (libsasl2)\" ON)\n    if(WITH_SASL_CYRUS)\n      list(APPEND BUILT_WITH \"SASL_CYRUS\")\n    endif()\n  endif()"
		_content "${_content}"
	)
	file(WRITE "${_patch_file}" "${_content}")
else()
	message(STATUS "librdkafka CMakeLists.txt: WITH_SASL_CYRUS option already present")
endif()
unset(_patch_file)
unset(_content)
unset(_already_patched)

# Patch 3: rdkafka_admin.c — guard RAND_priv_bytes against LibreSSL.
# LibreSSL reports OPENSSL_VERSION_NUMBER >= 0x10101000L (satisfying the guard)
# but does not implement this OpenSSL-internal function.
set(_patch_file "${RDKAFKA_SRC_BUILD}/src/rdkafka_admin.c")
file(READ "${_patch_file}" _content)
string(FIND "${_content}" "LIBRESSL_VERSION_NUMBER" _already_patched)
if(_already_patched EQUAL -1)
	message(STATUS "Patching librdkafka: rdkafka_admin.c RAND_priv_bytes guard")
	string(REPLACE
		"#if WITH_SSL && OPENSSL_VERSION_NUMBER >= 0x10101000L\n                unsigned char random_salt"
		"#if WITH_SSL && OPENSSL_VERSION_NUMBER >= 0x10101000L && !defined(LIBRESSL_VERSION_NUMBER)\n                unsigned char random_salt"
		_content "${_content}"
	)
	file(WRITE "${_patch_file}" "${_content}")
else()
	message(STATUS "librdkafka rdkafka_admin.c: RAND_priv_bytes guard already present")
endif()
unset(_patch_file)
unset(_content)
unset(_already_patched)
# ─────────────────────────────────────────────────────────────────────────────
if(CMAKE_GENERATOR_PLATFORM)
	set(PLATFORM_OPT "-A${CMAKE_GENERATOR_PLATFORM}")
elseif(CMAKE_VS_PLATFORM_NAME)
	set(PLATFORM_OPT "-A${CMAKE_VS_PLATFORM_NAME}")
else()
	set(PLATFORM_OPT "")
endif()
message("${CMAKE_COMMAND} ${RDKAFKA_CMAKE_OPTS} -G${CMAKE_GENERATOR} ${PLATFORM_OPT} -S ${RDKAFKA_SRC_BUILD} -Wno-dev")
execute_process(
	COMMAND ${CMAKE_COMMAND} ${RDKAFKA_CMAKE_OPTS} -G${CMAKE_GENERATOR} ${PLATFORM_OPT} -S ${RDKAFKA_SRC_BUILD}
	WORKING_DIRECTORY "${RDKAFKA_BUILD}"
	OUTPUT_FILE librdkafka-cmake-output.txt
	RESULT_VARIABLE EXEC_RESULT
)
if(EXEC_RESULT)
	message( FATAL_ERROR "Failed to run cmake for librdkafka submodule. See librdkafka-cmake-output.txt. Exiting")
endif()
set(CONFIGS "${CMAKE_BUILD_TYPE}")
if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Rel.*")
	list(APPEND CONFIGS "Release")
endif()
if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Deb.*")
	list(APPEND CONFIGS "Debug")
endif()
foreach(CONF ${CONFIGS})
	message("Building librdkafka dependency")
	execute_process(
		COMMAND ${CMAKE_COMMAND} --build ${RDKAFKA_BUILD} --config ${CONF} --parallel 8 --target install
		WORKING_DIRECTORY "${RDKAFKA_BUILD}"
		OUTPUT_FILE librdkafka-build-output.txt
		RESULT_VARIABLE EXEC_RESULT
	)
	if(EXEC_RESULT)
		message( FATAL_ERROR "Failed to build librdkafka submodule. See librdkafka-cmake-output.txt. Exiting")
	endif()
endforeach()
add_custom_target( build_librdkafka
	WORKING_DIRECTORY "${RDKAFKA_BUILD}"
	COMMAND ${CMAKE_COMMAND} --build ${RDKAFKA_BUILD} --config $<CONFIG> --parallel 8 --target install
)
set(RdKafka_CMAKE_MODULES "${RDKAFKA_HOME}/lib/cmake/RdKafka")
message("RDKAFKA_HOME: ${RDKAFKA_HOME}")
message("RdKafka_CMAKE_MODULES: ${RdKafka_CMAKE_MODULES}")
list(APPEND CMAKE_MODULE_PATH "${RdKafka_CMAKE_MODULES}")
set(RdKafka_DIR "${RdKafka_CMAKE_MODULES}")
find_package(RdKafka REQUIRED PATHS ${RDKAFKA_HOME} NO_DEFAULT_PATH)

# Interface fixup: strip cmake target names that originate from librdkafka's sub-build context
# and may not resolve correctly in ours; re-add them per-platform as appropriate.
# OpenSSL is always stripped — it is already linked into libODC.
# ZLIB / zstd per-platform:
#   Linux  – statically embedded in libODC.so via WHOLE_ARCHIVE; symbols exported and
#             resolved from libODC at dlopen time.  Do NOT add to KafkaPort link deps.
#   macOS  – ZLIB::ZLIB is system libz.dylib; libODC.dylib references it dynamically but
#             does not export its symbols; KafkaPort.so must link libz.dylib directly so
#             the macOS linker can resolve symbols from librdkafka at bundle build time.
#             zstd is disabled on macOS (WITH_ZSTD=OFF).
#   Windows – CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS cannot export symbols from /WHOLEARCHIVE
#             static libs; KafkaPort.dll must retain its own copy of zlib/zstd (Windows DLL
#             isolation means no duplicate-symbol crash, unlike macOS dyld4).
get_target_property(KAF_REQUIRED_LIBS RdKafka::rdkafka INTERFACE_LINK_LIBRARIES)
message("KAF_REQUIRED_LIBS BEFORE: ${KAF_REQUIRED_LIBS}")
list(FILTER KAF_REQUIRED_LIBS EXCLUDE REGEX "OpenSSL.*|ZLIB.*|zstd.*|ZSTD.*")
if(APPLE)
	# System libz.dylib: KafkaPort.so must link it directly.
	list(APPEND KAF_REQUIRED_LIBS ZLIB::ZLIB)
elseif(WIN32)
	# Keep zlib and zstd in KafkaPort.dll on Windows (see comment above).
	list(APPEND KAF_REQUIRED_LIBS ZLIB::ZLIB)
	if(TARGET zstd::libzstd_static)
		list(APPEND KAF_REQUIRED_LIBS zstd::libzstd_static)
	endif()
endif()
message("KAF_REQUIRED_LIBS AFTER: ${KAF_REQUIRED_LIBS}")
set_target_properties(RdKafka::rdkafka PROPERTIES INTERFACE_LINK_LIBRARIES "${KAF_REQUIRED_LIBS}")

#another hack to mark the librdkafka include directory as a system directory
# so that we don't get warnings about it
get_target_property(KAF_INCLUDE_DIRS RdKafka::rdkafka INTERFACE_INCLUDE_DIRECTORIES)
message("Setting include dirs as 'SYSTEM' to supress 3rd party warnings: ${KAF_INCLUDE_DIRS}")
set_target_properties(RdKafka::rdkafka PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${KAF_INCLUDE_DIRS}")
