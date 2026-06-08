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

# build_libffi.cmake
# Vendors libffi 3.4.6 as a static PIC library via its autotools build system.
#
# Escape hatch: if LibFFI_LIBRARY is already defined and non-empty, the build
# is skipped entirely (caller can pre-set it to use a system libffi).
#
# After a successful build, sets in the calling scope:
#   LibFFI_LIBRARY     - absolute path to libffi.a
#   LibFFI_INCLUDE_DIR - directory containing ffi.h
#
# Requires: CC-compatible C compiler on PATH, make.
# Out-of-source autotools build: source extracted to Code/libffi/libffi-<ver>/,
# built in Code/libffi/build/, installed to Code/libffi/install/.

if(DEFINED LibFFI_LIBRARY AND NOT "${LibFFI_LIBRARY}" STREQUAL "")
	message(STATUS "build_libffi.cmake: LibFFI_LIBRARY already set to '${LibFFI_LIBRARY}' — skipping vendored build")
	return()
endif()

if(WIN32)
	message(STATUS "build_libffi.cmake: autotools not available on Windows — skipping vendored libffi build. _ctypes will be disabled unless LibFFI_LIBRARY is pre-set.")
	return()
endif()

set(LIBFFI_VERSION "3.4.6")
set(LIBFFI_SHA256  "b0dea9df23c863a7a50e825440f3ebffabd65df1497108e5d437747843895a4e")

set(LIBFFI_VENDOR_DIR "${CMAKE_BINARY_DIR}/Code/libffi")
set(LIBFFI_SOURCE     "${LIBFFI_VENDOR_DIR}/libffi-${LIBFFI_VERSION}")
set(LIBFFI_BUILD      "${LIBFFI_VENDOR_DIR}/build")
set(LIBFFI_INSTALL    "${LIBFFI_VENDOR_DIR}/install")

if(NOT EXISTS "${LIBFFI_INSTALL}/include/ffi.h")

	# ------------------------------------------------------------------ #
	# Download + extract                                                   #
	# ------------------------------------------------------------------ #
	if(NOT EXISTS "${LIBFFI_SOURCE}/configure")
		set(_LIBFFI_TARBALL "${LIBFFI_VENDOR_DIR}/libffi-${LIBFFI_VERSION}.tar.gz")
		if(NOT EXISTS "${_LIBFFI_TARBALL}")
			message(STATUS "Downloading libffi ${LIBFFI_VERSION}...")
			file(MAKE_DIRECTORY "${LIBFFI_VENDOR_DIR}")
			file(DOWNLOAD
				"https://github.com/libffi/libffi/releases/download/v${LIBFFI_VERSION}/libffi-${LIBFFI_VERSION}.tar.gz"
				"${_LIBFFI_TARBALL}"
				EXPECTED_HASH SHA256=${LIBFFI_SHA256}
				SHOW_PROGRESS
				STATUS DL_STATUS
			)
			list(GET DL_STATUS 0 DL_RESULT)
			if(DL_RESULT)
				list(GET DL_STATUS 1 DL_MSG)
				message(FATAL_ERROR "Failed to download libffi ${LIBFFI_VERSION}: ${DL_MSG}")
			endif()
		endif()
		message(STATUS "Extracting libffi ${LIBFFI_VERSION}...")
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E tar xzf "${_LIBFFI_TARBALL}"
			WORKING_DIRECTORY "${LIBFFI_VENDOR_DIR}"
			RESULT_VARIABLE EXEC_RESULT
		)
		if(EXEC_RESULT)
			message(FATAL_ERROR "Failed to extract libffi tarball.")
		endif()
		unset(_LIBFFI_TARBALL)
	endif()

	# ------------------------------------------------------------------ #
	# Autotools configure (out-of-source)                                  #
	# ------------------------------------------------------------------ #
	file(MAKE_DIRECTORY "${LIBFFI_BUILD}")
	message(STATUS "Configuring libffi ${LIBFFI_VERSION}...")
	execute_process(
		COMMAND "${LIBFFI_SOURCE}/configure"
			--prefix=${LIBFFI_INSTALL}
			--enable-static
			--disable-shared
			--with-pic
			CC=${CMAKE_C_COMPILER}
		WORKING_DIRECTORY "${LIBFFI_BUILD}"
		OUTPUT_FILE libffi-configure-output.txt
		ERROR_FILE  libffi-configure-error.txt
		RESULT_VARIABLE EXEC_RESULT
	)
	if(EXEC_RESULT)
		message(FATAL_ERROR
			"Failed to configure libffi. "
			"See ${LIBFFI_BUILD}/libffi-configure-output.txt and ${LIBFFI_BUILD}/libffi-configure-error.txt")
	endif()

	# ------------------------------------------------------------------ #
	# Build + install                                                       #
	# ------------------------------------------------------------------ #
	message(STATUS "Building libffi ${LIBFFI_VERSION}...")
	execute_process(
		COMMAND make install -j8
		WORKING_DIRECTORY "${LIBFFI_BUILD}"
		OUTPUT_FILE libffi-build-output.txt
		ERROR_FILE  libffi-build-error.txt
		RESULT_VARIABLE EXEC_RESULT
	)
	if(EXEC_RESULT)
		message(FATAL_ERROR
			"Failed to build libffi. "
			"See ${LIBFFI_BUILD}/libffi-build-output.txt and ${LIBFFI_BUILD}/libffi-build-error.txt")
	endif()

endif()

set(LibFFI_LIBRARY    "${LIBFFI_INSTALL}/lib/libffi.a")
set(LibFFI_INCLUDE_DIR "${LIBFFI_INSTALL}/include")
message(STATUS "Vendored libffi ${LIBFFI_VERSION}: ${LibFFI_LIBRARY}")
