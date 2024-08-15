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

set(RDKAFKA_SOURCE "${CMAKE_SOURCE_DIR}/Code/submodules/librdkafka")
set(RDKAFKA_BUILD "${CMAKE_BINARY_DIR}/Code/submodules/librdkafka")
mark_as_advanced(FORCE RDKAFKA_SOURCE)
mark_as_advanced(FORCE RDKAFKA_BUILD)
set(RDKAFKA_HOME "${RDKAFKA_BUILD}/install")
set(
	RDKAFKA_CMAKE_OPTS
		-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DRDKAFKA_BUILD_STATIC=ON
		-DENABLE_LZ4_EXT=OFF
		-DRDKAFKA_BUILD_EXAMPLES=OFF
		-DRDKAFKA_BUILD_TESTS=OFF
		-DOPENSSL_USE_STATIC_LIBS=TRUE
		-DOPENSSL_MSVC_STATIC_RT=TRUE
		-DWITH_SSL=${ODC_ASIO_SSL}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
		-DCMAKE_INSTALL_PREFIX=${RDKAFKA_HOME}/
		-DCMAKE_DEBUG_POSTFIX=${CMAKE_DEBUG_POSTFIX}
		-DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}
	CACHE STRING "cmake options to use when building librdkafka submodule"
)
if(NOT EXISTS "${RDKAFKA_SOURCE}/.git")
	execute_process(COMMAND git submodule update --init -- Code/submodules/librdkafka
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
if(NOT EXISTS "${RDKAFKA_BUILD}")
	file(MAKE_DIRECTORY "${RDKAFKA_BUILD}")
endif()
if(CMAKE_GENERATOR_PLATFORM)
	set(PLATFORM_OPT "-A${CMAKE_GENERATOR_PLATFORM}")
elseif(CMAKE_VS_PLATFORM_NAME)
	set(PLATFORM_OPT "-A${CMAKE_VS_PLATFORM_NAME}")
else()
	set(PLATFORM_OPT "")
endif()
message("${CMAKE_COMMAND} ${RDKAFKA_CMAKE_OPTS} -G${CMAKE_GENERATOR} ${PLATFORM_OPT} -S ${RDKAFKA_SOURCE}")
execute_process(
	COMMAND ${CMAKE_COMMAND} ${RDKAFKA_CMAKE_OPTS} -G${CMAKE_GENERATOR} ${PLATFORM_OPT} -S ${RDKAFKA_SOURCE}
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
#set these here for consistent dependency discovery, since we set them in the librdkafka build
set(OPENSSL_USE_STATIC_LIBS TRUE)
set(OPENSSL_MSVC_STATIC_RT TRUE)
find_package(RdKafka REQUIRED PATHS ${RDKAFKA_HOME} NO_DEFAULT_PATH)

if(ODC_ASIO_SSL)
	#big fat hack to remove OpenSSL::SSL and OpenSSL::Crypto from the librdkafka interface libs
	# because there's no way to tell librdkafka that it's compiled/linked into ODC already
	get_target_property(KAF_REQUIRED_LIBS RdKafka::rdkafka INTERFACE_LINK_LIBRARIES)
	message("KAF_REQUIRED_LIBS BEFORE: ${KAF_REQUIRED_LIBS}")
	list(FILTER KAF_REQUIRED_LIBS EXCLUDE REGEX "OpenSSL.*")
	message("KAF_REQUIRED_LIBS AFTER: ${KAF_REQUIRED_LIBS}")
	set_target_properties(RdKafka::rdkafka PROPERTIES INTERFACE_LINK_LIBRARIES "${KAF_REQUIRED_LIBS}")
endif()

