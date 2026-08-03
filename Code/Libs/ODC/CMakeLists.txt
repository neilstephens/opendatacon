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
project(ODC)

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def ${CMAKE_SOURCE_DIR}/include/opendatacon/*.h)

add_library(${PROJECT_NAME} SHARED ${${PROJECT_NAME}_SRC})
target_include_directories(${PROJECT_NAME} PUBLIC
	"../JSON"
	"${CMAKE_SOURCE_DIR}/include"
	"${SPDLOG_HOME}/include"
	"${ASIO_HOME}/include")

set(ODC_ASIO_SSL OFF CACHE BOOL "Enable TLS/SSL is ASIO compiled into libODC (requires OpenSSL)")
if(FULL)
	set(ODC_ASIO_SSL ON CACHE BOOL "Enable TLS/SSL is ASIO compiled into libODC (requires OpenSSL)" FORCE)
endif()

if(ODC_ASIO_SSL)
	message("OpenSSL root dir: ${OPENSSL_ROOT_DIR}")

	# Per-platform OpenSSL linking
	if(WIN32)
		# Static linking on Windows (self-contained installer, no redist needed)
		set(OPENSSL_USE_STATIC_LIBS TRUE)
		set(OPENSSL_MSVC_STATIC_RT TRUE)
	else()
		# Dynamic linking on Linux/macOS (distro .so is PIC-safe, auto-bundled by file(GET_RUNTIME_DEPENDENCIES))
		set(OPENSSL_USE_STATIC_LIBS FALSE)
		set(OPENSSL_MSVC_STATIC_RT FALSE)
	endif()
	set(OPENSSL_USE_STATIC_LIBS ${OPENSSL_USE_STATIC_LIBS} PARENT_SCOPE)
	set(OPENSSL_MSVC_STATIC_RT ${OPENSSL_MSVC_STATIC_RT} PARENT_SCOPE)

	find_package(OpenSSL 1.1.1 REQUIRED)

	# zlib and zstd are needed by OpenSSL's compression API regardless of static/dynamic linking
	find_package(ZLIB QUIET)
	if(ZLIB_FOUND)
		message(STATUS "Found zlib")
		set(ZLIB_LIB ZLIB::ZLIB)
	endif()

	find_package(zstd QUIET)
	if(zstd_FOUND)
		message(STATUS "Found zstd")
		set(ZSTD_LIB $<IF:$<TARGET_EXISTS:zstd::libzstd_static>,zstd::libzstd_static,zstd::libzstd_shared>)
	endif()

	add_definitions(-DODC_ASIO_SSL)
	message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
	message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")
	if(WIN32)
		set(ADD_LIBS Crypt32)
	endif()
endif()

target_link_libraries(${PROJECT_NAME} JSON $<$<BOOL:${ODC_ASIO_SSL}>:OpenSSL::SSL> $<$<BOOL:${ODC_ASIO_SSL}>:${ZLIB_LIB}> $<$<BOOL:${ODC_ASIO_SSL}>:${ZSTD_LIB}> ${ADD_LIBS} ${RT} ${FS})

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_SHARED} ARCHIVE DESTINATION ${INSTALLDIR_LIBS} RUNTIME DESTINATION ${INSTALLDIR_SHARED})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER libs)
