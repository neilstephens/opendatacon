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

# compile-python.cmake
# Vendored CPython 3.12 build using upstream python-cmake-buildsystem.
#
# Downloads:
#   - python-cmake-buildsystem from GitHub (pinned commit, SHA256-verified)
#   - CPython 3.12.13 source tarball from python.org (SHA256-verified)
#
# Builds a static libpython with all enabled extensions compiled in
# (BUILD_EXTENSIONS_AS_BUILTIN=ON, BUILD_LIBPYTHON_SHARED=OFF).
#
# Disabled extensions: bz2 (_bz2), readline, curses, gdbm, sqlite3, lzma.
# Also disabled: _ssl and _hashlib/_hashopenssl — CPython 3.12 uses
#   OpenSSL 3.x APIs (EVP_MD_FLAG_XOF, X509_STORE_lock, etc.) absent from
#   LibreSSL 4.x; disabling avoids patching CPython source.  Pure-C hash
#   fallbacks (_md5, _sha256, _sha512, _sha3, _blake2) are still built.
# Enabled extensions: _ctypes (vendored libffi 3.4.6), zlib, _decimal
#   (bundled libmpdec), pyexpat (bundled expat), and all pure-C builtins.
#
# After the build, sets in the calling scope:
#   PYTHON_LIBRARIES   - cmake target 'libpython-static' (IMPORTED STATIC)
#   PYTHON_INCLUDE_DIRS - installed Python headers directory
#   PYTHON_HOME        - install prefix (cache PATH)
#
# Also defines cmake targets: build_python, copy-python-files
# Also calls: install(DIRECTORY ...) for the Python stdlib
# Also calls: add_definitions(-DPYTHON_LIBDIR="Python${MAJMIN_COMPACT}")
#
# Guards: skip download/configure/build if stdlib already present at
#   ${PCBS_INSTALL}/lib/python${MAJMIN}/os.py

if(USE_PYTHON_SUBMODULE)

	# ------------------------------------------------------------------ #
	# Pinned versions                                                      #
	# ------------------------------------------------------------------ #
	set(PYTHON_VERSION "3.12.13" CACHE STRING "CPython version to build when USE_PYTHON_SUBMODULE=ON")

	# python-cmake-buildsystem: upstream master at this commit
	# SHA256 of https://github.com/python-cmake-buildsystem/python-cmake-buildsystem/archive/<COMMIT>.tar.gz
	set(PCBS_COMMIT  "a206a24735b9fa01cd0f7309051576f6c58a46c9")
	set(PCBS_SHA256  "ecaaf5957b8ab2cff9bdb06d7aefddfb3190ec1abfa0905aed80c3658e7456e8")

	# CPython 3.12.13 gzip tarball SHA256 (from python.org)
	set(CPYTHON_SHA256 "0816c4761c97ecdb3f50a3924de0a93fd78cb63ee8e6c04201ddfaedca500b0b")

	# ------------------------------------------------------------------ #
	# Derived version strings                                              #
	# ------------------------------------------------------------------ #
	string(REPLACE "." ";" _py_ver_list "${PYTHON_VERSION}")
	list(GET _py_ver_list 0 PY_MAJOR)
	list(GET _py_ver_list 1 PY_MINOR)
	set(PY_MAJMIN          "${PY_MAJOR}.${PY_MINOR}")
	set(PY_MAJMIN_COMPACT  "${PY_MAJOR}${PY_MINOR}")

	# ------------------------------------------------------------------ #
	# Directory layout (all under cmake binary dir)                       #
	# ------------------------------------------------------------------ #
	set(PYTHON_VENDOR_DIR "${CMAKE_BINARY_DIR}/Code/python-vendored")
	set(PCBS_SOURCE       "${PYTHON_VENDOR_DIR}/pcbs-src")
	set(CPYTHON_SOURCE    "${PYTHON_VENDOR_DIR}/Python-${PYTHON_VERSION}")
	set(PCBS_BUILD        "${PYTHON_VENDOR_DIR}/pcbs-build")
	set(PCBS_INSTALL      "${PCBS_BUILD}/install")
	set(PYTHON_HOME       "${PCBS_INSTALL}" CACHE PATH ${PYTHON_HOME_INSTRUCTIONS} FORCE)
	mark_as_advanced(FORCE PYTHON_HOME)

	# ------------------------------------------------------------------ #
	# Download + extract python-cmake-buildsystem                         #
	# ------------------------------------------------------------------ #
	if(NOT EXISTS "${PCBS_SOURCE}/CMakeLists.txt")
		file(MAKE_DIRECTORY "${PYTHON_VENDOR_DIR}")
		set(PCBS_TARBALL "${PYTHON_VENDOR_DIR}/pcbs-${PCBS_COMMIT}.tar.gz")
		if(NOT EXISTS "${PCBS_TARBALL}")
			message(STATUS "Downloading python-cmake-buildsystem (commit ${PCBS_COMMIT})...")
			file(DOWNLOAD
				"https://github.com/python-cmake-buildsystem/python-cmake-buildsystem/archive/${PCBS_COMMIT}.tar.gz"
				"${PCBS_TARBALL}"
				EXPECTED_HASH SHA256=${PCBS_SHA256}
				SHOW_PROGRESS
				STATUS DL_STATUS
			)
			list(GET DL_STATUS 0 DL_RESULT)
			if(DL_RESULT)
				list(GET DL_STATUS 1 DL_MSG)
				message(FATAL_ERROR "Failed to download python-cmake-buildsystem: ${DL_MSG}")
			endif()
		endif()
		message(STATUS "Extracting python-cmake-buildsystem...")
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E tar xzf "${PCBS_TARBALL}"
			WORKING_DIRECTORY "${PYTHON_VENDOR_DIR}"
			RESULT_VARIABLE EXEC_RESULT
		)
		if(EXEC_RESULT)
			message(FATAL_ERROR "Failed to extract python-cmake-buildsystem tarball.")
		endif()
		# GitHub archives use "reponame-<commit>" as the extracted dir name
		file(GLOB _PCBS_EXTRACTED "${PYTHON_VENDOR_DIR}/python-cmake-buildsystem-*")
		list(LENGTH _PCBS_EXTRACTED _PCBS_LEN)
		if(_PCBS_LEN GREATER 0)
			list(GET _PCBS_EXTRACTED 0 _PCBS_FIRST)
			file(RENAME "${_PCBS_FIRST}" "${PCBS_SOURCE}")
		else()
			message(FATAL_ERROR "python-cmake-buildsystem archive extracted to an unexpected directory name in ${PYTHON_VENDOR_DIR}")
		endif()
	endif()

	# ------------------------------------------------------------------ #
	# Download + extract CPython source                                    #
	# ------------------------------------------------------------------ #
	if(NOT EXISTS "${CPYTHON_SOURCE}/pyconfig.h.in")
		set(CPYTHON_TARBALL "${PYTHON_VENDOR_DIR}/Python-${PYTHON_VERSION}.tgz")
		if(NOT EXISTS "${CPYTHON_TARBALL}")
			message(STATUS "Downloading CPython ${PYTHON_VERSION}...")
			file(DOWNLOAD
				"https://www.python.org/ftp/python/${PYTHON_VERSION}/Python-${PYTHON_VERSION}.tgz"
				"${CPYTHON_TARBALL}"
				EXPECTED_HASH SHA256=${CPYTHON_SHA256}
				SHOW_PROGRESS
				STATUS DL_STATUS
			)
			list(GET DL_STATUS 0 DL_RESULT)
			if(DL_RESULT)
				list(GET DL_STATUS 1 DL_MSG)
				message(FATAL_ERROR "Failed to download CPython ${PYTHON_VERSION}: ${DL_MSG}")
			endif()
		endif()
		message(STATUS "Extracting CPython ${PYTHON_VERSION}...")
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E tar xzf "${CPYTHON_TARBALL}"
			WORKING_DIRECTORY "${PYTHON_VENDOR_DIR}"
			RESULT_VARIABLE EXEC_RESULT
		)
		if(EXEC_RESULT)
			message(FATAL_ERROR "Failed to extract CPython ${PYTHON_VERSION} tarball.")
		endif()
	endif()

	# ------------------------------------------------------------------ #
	# Vendored deps: ensure zlib and libffi are available                 #
	# ------------------------------------------------------------------ #
	# build_zlib.cmake is idempotent: skips if ZLIB_ROOT already set.
	# Including it here ensures Python can use vendored zlib even when
	# KAFKAPORT is not enabled (which is the only other consumer).
	if(NOT (DEFINED ZLIB_ROOT AND NOT "${ZLIB_ROOT}" STREQUAL ""))
		include(${CMAKE_SOURCE_DIR}/Code/cmake/build_zlib.cmake)
	endif()
	# build_libffi.cmake is idempotent: skips if LibFFI_LIBRARY already set.
	# libffi enables the _ctypes extension (vendored, PIC static lib).
	if(NOT (DEFINED LibFFI_LIBRARY AND NOT "${LibFFI_LIBRARY}" STREQUAL ""))
		include(${CMAKE_SOURCE_DIR}/Code/cmake/build_libffi.cmake)
	endif()
	# Note: LibreSSL is NOT needed for the vendored Python build.
	# CPython 3.12 _ssl/_hashlib use OpenSSL 3.x APIs absent from LibreSSL 4.x,
	# so USE_SYSTEM_OpenSSL=OFF is passed to PCBS (no _ssl or _hashlib built).

	# ------------------------------------------------------------------ #
	# Collect PCBS cmake options                                          #
	# ------------------------------------------------------------------ #
	if(CMAKE_GENERATOR_PLATFORM)
		set(_PLATFORM_OPT "-A${CMAKE_GENERATOR_PLATFORM}")
	elseif(CMAKE_VS_PLATFORM_NAME)
		set(_PLATFORM_OPT "-A${CMAKE_VS_PLATFORM_NAME}")
	else()
		set(_PLATFORM_OPT "")
	endif()

	set(
		PCBS_CMAKE_OPTS
			-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
			-DCMAKE_INSTALL_PREFIX=${PCBS_INSTALL}
			-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
			-DCMAKE_POSITION_INDEPENDENT_CODE=ON
			-DPYTHON_VERSION=${PYTHON_VERSION}
			# Tell PCBS where the CPython source lives; skip its own download
			-DDOWNLOAD_SOURCES=OFF
			-DSRC_DIR=${CPYTHON_SOURCE}
			# Static libpython with all extensions compiled in
			-DBUILD_LIBPYTHON_SHARED=OFF
			-DBUILD_EXTENSIONS_AS_BUILTIN=ON
			# No test/manual install cruft
			-DINSTALL_TEST=OFF
			-DINSTALL_MANUAL=OFF
			-DINSTALL_DEVELOPMENT=ON
		# Use system-style finds, but override per-dep below
		-DUSE_SYSTEM_LIBRARIES=ON
		# OpenSSL: disabled — CPython 3.12 _ssl/_hashlib need OpenSSL 3.x APIs
		# not present in LibreSSL; pure-C hash fallbacks are built instead.
		-DUSE_SYSTEM_OpenSSL=OFF
		# zlib (vendored)
		-DUSE_SYSTEM_ZLIB=ON
		-DZLIB_ROOT=${ZLIB_ROOT}
		# Bundled libmpdec (from CPython source tree) — this is PCBS default
		-DUSE_SYSTEM_LIBMPDEC=OFF
		# Bundled expat (from CPython source tree)
		-DUSE_SYSTEM_EXPAT=OFF
		# libffi (vendored) — provides _ctypes
		-DUSE_SYSTEM_LibFFI=ON
		-DLibFFI_LIBRARY=${LibFFI_LIBRARY}
		-DLibFFI_INCLUDE_DIR=${LibFFI_INCLUDE_DIR}
		-DUSE_SYSTEM_BZip2=OFF
		-DUSE_SYSTEM_Curses=OFF
		-DUSE_SYSTEM_READLINE=OFF
		-DUSE_SYSTEM_TCL=OFF
		-DUSE_SYSTEM_GDBM=OFF
		-DUSE_SYSTEM_SQLite3=OFF
		# Disable CPython test-only C extensions: not needed for PyPort and
		# _testinternalcapi has a circular bootstrap dependency in 3.12 that
		# causes a link failure when BUILD_EXTENSIONS_AS_BUILTIN=ON.
		-DENABLE_TESTINTERNALCAPI=OFF
		-DENABLE_TESTCAPI=OFF
		-DENABLE_TESTBUFFER=OFF
		-DENABLE_TESTIMPORTMULTIPLE=OFF
		-DENABLE_TESTMULTIPHASE=OFF
		-DENABLE_TESTCLINIC=OFF
		-DENABLE_TESTSINGLEPHASE=OFF
		-DENABLE_XXTESTFUZZ=OFF
		CACHE STRING "cmake options for python-cmake-buildsystem (vendored CPython build)"
		FORCE
	)

	# ------------------------------------------------------------------ #
	# Forward cross-compilation and host-platform settings to PCBS        #
	# These are appended after the cache set so they do not pollute the   #
	# inspectable cache entry but ARE present in the local list used by   #
	# the execute_process call below.                                      #
	# ------------------------------------------------------------------ #
	# Compiler flags: needed for e.g. armhf where CMAKE_C_FLAGS carries
	# "-target arm-linux-gnueabihf -march=armv6 -mcpu=arm1176jzf-s ..."
	if(CMAKE_C_FLAGS)
		list(APPEND PCBS_CMAKE_OPTS "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}")
	endif()
	if(CMAKE_CXX_FLAGS)
		list(APPEND PCBS_CMAKE_OPTS "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}")
	endif()
	# macOS: propagate SDK root and deployment target so that PCBS's
	# inner cmake inherits the same Xcode/CLT sysroot as the outer build.
	if(APPLE)
		if(CMAKE_OSX_SYSROOT)
			list(APPEND PCBS_CMAKE_OPTS "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}")
		endif()
		if(CMAKE_OSX_DEPLOYMENT_TARGET)
			list(APPEND PCBS_CMAKE_OPTS "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
		endif()
		if(CMAKE_OSX_ARCHITECTURES)
			list(APPEND PCBS_CMAKE_OPTS "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}")
		endif()
	endif()

	# ------------------------------------------------------------------ #
	# Configure + build (guarded by presence of installed stdlib)         #
	# ------------------------------------------------------------------ #
	if(NOT EXISTS "${PCBS_BUILD}")
		file(MAKE_DIRECTORY "${PCBS_BUILD}")
	endif()

	if(NOT EXISTS "${PCBS_INSTALL}/lib/python${PY_MAJMIN}/os.py")
		message("Configuring python-cmake-buildsystem for CPython ${PYTHON_VERSION}")
		execute_process(
			COMMAND ${CMAKE_COMMAND} ${PCBS_CMAKE_OPTS}
				-G${CMAKE_GENERATOR} ${_PLATFORM_OPT}
				-S ${PCBS_SOURCE}
			WORKING_DIRECTORY "${PCBS_BUILD}"
			OUTPUT_FILE pcbs-cmake-output.txt
			ERROR_FILE pcbs-cmake-error.txt
			RESULT_VARIABLE EXEC_RESULT
		)
		if(EXEC_RESULT)
			message(FATAL_ERROR "Failed to configure python-cmake-buildsystem. See ${PCBS_BUILD}/pcbs-cmake-output.txt and ${PCBS_BUILD}/pcbs-cmake-error.txt")
		endif()

		set(CONFIGS "${CMAKE_BUILD_TYPE}")
		if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Rel.*")
			list(APPEND CONFIGS "Release")
		endif()
		if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Deb.*")
			list(APPEND CONFIGS "Debug")
		endif()
		foreach(CONF ${CONFIGS})
			message("Building CPython ${PYTHON_VERSION} (${CONF}) — this may take several minutes")
			execute_process(
				COMMAND ${CMAKE_COMMAND} --build ${PCBS_BUILD}
					--config ${CONF} --parallel 8 --target install
				WORKING_DIRECTORY "${PCBS_BUILD}"
				OUTPUT_FILE pcbs-build-output.txt
				RESULT_VARIABLE EXEC_RESULT
			)
			if(EXEC_RESULT)
				message(FATAL_ERROR "Failed to build CPython ${PYTHON_VERSION}. See ${PCBS_BUILD}/pcbs-build-output.txt")
			endif()
		endforeach()
	else()
		message(STATUS "CPython ${PYTHON_VERSION} already built at ${PCBS_INSTALL}")
	endif()

	# ------------------------------------------------------------------ #
	# Incremental rebuild target                                           #
	# ------------------------------------------------------------------ #
	add_custom_target(build_python
		WORKING_DIRECTORY "${PCBS_BUILD}"
		COMMAND ${CMAKE_COMMAND} --build ${PCBS_BUILD}
			--config $<CONFIG> --parallel 8 --target install
	)

	# ------------------------------------------------------------------ #
	# Import Python cmake targets from install tree                       #
	# ------------------------------------------------------------------ #
	# PythonConfig.cmake (install-tree) defines the IMPORTED libpython-static
	# target and sets PYTHON_LIBRARIES + PYTHON_INCLUDE_DIRS.
	include(${PCBS_INSTALL}/share/python${PY_MAJMIN}/PythonConfig.cmake)
	# Ensure PYTHON_LIBRARIES is the static target (BUILD_LIBPYTHON_SHARED=OFF
	# means PythonConfig.cmake already sets it to libpython-static, but be explicit)
	set(PYTHON_LIBRARIES libpython-static)

	# ------------------------------------------------------------------ #
	# Install Python stdlib alongside the ODC binaries                    #
	# ------------------------------------------------------------------ #
	install(
		DIRECTORY   ${PCBS_INSTALL}/lib/python${PY_MAJMIN}/
		DESTINATION ${INSTALLDIR_SHARED}/Python${PY_MAJMIN_COMPACT}/lib/python${PY_MAJMIN}/
	)
	add_definitions(-DPYTHON_LIBDIR="Python${PY_MAJMIN_COMPACT}")

	# Copy stdlib into the build-tree output dir so tests can run in-place
	add_custom_target(copy-python-files ALL
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			${PCBS_INSTALL}/lib/python${PY_MAJMIN}
			${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Python${PY_MAJMIN_COMPACT}
		DEPENDS build_python
	)

	# Collect any .exe helpers bundled in the stdlib (Windows)
	file(GLOB_RECURSE PYTHON_EXES ${PCBS_INSTALL}/lib/python${PY_MAJMIN}/*.exe)
	foreach(python_exe ${PYTHON_EXES})
		get_filename_component(PYTHON_EXE_NAME ${python_exe} NAME)
		set(PYTHON_EXE_NAMES ${PYTHON_EXE_NAMES} ${PYTHON_EXE_NAME})
	endforeach()

endif()
