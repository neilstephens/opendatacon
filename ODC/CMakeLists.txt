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

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def ../include/opendatacon/*.h)

add_library(${PROJECT_NAME} SHARED ${${PROJECT_NAME}_SRC})
add_dependencies(${PROJECT_NAME} version) #make sure version info is up-to-date first
target_include_directories(${PROJECT_NAME} PUBLIC
	"../JSON"
	"../include"
	"${SPDLOG_HOME}/include"
	"${ASIO_HOME}/asio/include")

set(ODC_ASIO_SSL OFF CACHE BOOL "Enable TLS/SSL is ASIO compiled into libODC (requires OpenSSL)")
if(FULL)
	set(ODC_ASIO_SSL ON CACHE BOOL "Enable TLS/SSL is ASIO compiled into libODC (requires OpenSSL)" FORCE)
endif()

if(ODC_ASIO_SSL)
	message("OpenSSL root dir: ${OPENSSL_ROOT_DIR}")
	set(OPENSSL_USE_STATIC_LIBS TRUE)
	set(OPENSSL_MSVC_STATIC_RT TRUE)
	find_package(OpenSSL 1.1.1 REQUIRED)
	add_definitions(-DODC_ASIO_SSL)
	message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
	message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")
	if(WIN32)
		set(ADD_LIBS Crypt32)
	endif()
endif()

target_link_libraries(${PROJECT_NAME} JSON $<$<BOOL:${ODC_ASIO_SSL}>:OpenSSL::SSL> ${ADD_LIBS} ${LIBCXX} ${RT})

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_SHARED} ARCHIVE DESTINATION ${INSTALLDIR_LIBS} RUNTIME DESTINATION ${INSTALLDIR_SHARED})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER libs)

install(CODE
"
	set(BUNDLE_DEPS_LIST \${BUNDLE_DEPS_LIST}
		\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_SHARED}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}\${BUNDLE_LIB_POSTFIX}${CMAKE_SHARED_LIBRARY_SUFFIX}
	)
")
