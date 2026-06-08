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

if(ODC_ASIO_SSL)
	message("OpenSSL root dir: ${OPENSSL_ROOT_DIR}")

	set(OPENSSL_USE_STATIC_LIBS TRUE)
	set(OPENSSL_MSVC_STATIC_RT TRUE)

	find_package(OpenSSL 1.1.1 REQUIRED)

	add_definitions(-DODC_ASIO_SSL)
	message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
	message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")
	if(WIN32)
		# Crypt32: required by LibreSSL's TLS code on Windows
		# bcrypt: required by LibreSSL's getentropy_win.c (BCryptGenRandom)
		set(ADD_LIBS Crypt32 bcrypt)
	endif()
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC JSON $<$<BOOL:${ODC_ASIO_SSL}>:OpenSSL::SSL> ${ADD_LIBS} ${RT} ${FS})
# Linux only: embed entire vendored zlib and zstd static archives into libODC.so so that
# their symbols are exported from libODC.so and KafkaPort.so can resolve them at dlopen
# time without carrying its own copies.
# --whole-archive (via $<LINK_LIBRARY:WHOLE_ARCHIVE,...>, CMake >= 3.24) forces inclusion of
# all object files from the static archives even though ODC itself does not call zlib/zstd.
# macOS: excluded — ZLIB::ZLIB is system libz.dylib (shared); WHOLE_ARCHIVE is for static
#         libs; KafkaPort links libz.dylib directly (see build_librdkafka.cmake APPLE guard).
# Windows: excluded — CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS only scans ODC's own .obj files, not
#         objects embedded via /WHOLEARCHIVE, so symbols would not be exported from the .def
#         file; KafkaPort.dll retains its own copy of zlib/zstd (Windows DLL isolation
#         prevents duplicate-symbol crashes that afflict macOS dyld4).
if(KAFKAPORT AND UNIX AND NOT APPLE)
	if(CMAKE_VERSION VERSION_LESS "3.24")
		message(FATAL_ERROR
			"KAFKAPORT on Linux requires CMake >= 3.24: "
			"LINK_LIBRARY:WHOLE_ARCHIVE is needed to export vendored zlib/zstd "
			"symbols from libODC.so so that KafkaPort resolves them at runtime.")
	endif()
	# Prefer ZLIB::ZLIBSTATIC (created by vendored zlib's cmake config package) so we
	# always get the static archive even when ZLIB::ZLIB resolves to the shared lib.
	if(TARGET ZLIB::ZLIBSTATIC)
		target_link_libraries(${PROJECT_NAME} PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,ZLIB::ZLIBSTATIC>)
	else()
		target_link_libraries(${PROJECT_NAME} PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,ZLIB::ZLIB>)
	endif()
	if(TARGET zstd::libzstd_static)
		target_link_libraries(${PROJECT_NAME} PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,zstd::libzstd_static>)
	endif()
endif()

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_SHARED} ARCHIVE DESTINATION ${INSTALLDIR_LIBS} RUNTIME DESTINATION ${INSTALLDIR_SHARED})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER libs)


