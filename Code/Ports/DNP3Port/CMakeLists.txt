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
project(DNP3Port)

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def)

# Locate opendnp3
option(USE_DNP3_SUBMODULE "Use git submodule to download and build opendnp3 libraries" ON)
set(DNP3_HOME_INSTRUCTIONS "Choose the location of opendnp3: where it was installed - there should be lib and include directories within")

set(DNP3_TLS OFF CACHE BOOL "Build TLS client/server support in opendnp3 (requires ODC_ASIO_SSL)")
if(FULL)
	set(DNP3_TLS ON CACHE BOOL "Build TLS client/server support in opendnp3 (requires ODC_ASIO_SSL)" FORCE)
endif()
if(DNP3_TLS)
	if(NOT ODC_ASIO_SSL)
		message(FATAL_ERROR "DNP3_TLS requires ODC_ASIO_SSL. Enable that option first")
	endif()
endif()

if(USE_DNP3_SUBMODULE)
	set(DNP3_SOURCE "${CMAKE_SOURCE_DIR}/Code/submodules/dnp3")
	set(DNP3_BUILD "${CMAKE_BINARY_DIR}/Code/submodules/dnp3")
	mark_as_advanced(FORCE DNP3_SOURCE)
	mark_as_advanced(FORCE DNP3_BUILD)
	set(DNP3_HOME "${DNP3_BUILD}/install" CACHE PATH ${DNP3_HOME_INSTRUCTIONS} FORCE)
	set(
		DNP3_CMAKE_OPTS
			-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
			-DDNP3_STATIC_LIBS=ON
			-DOPENSSL_USE_STATIC_LIBS=TRUE
			"-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS} -fPIC"
			-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
			-DCMAKE_INSTALL_PREFIX=${DNP3_HOME}/
			-DCMAKE_DEBUG_POSTFIX=${CMAKE_DEBUG_POSTFIX}
			-DFETCHCONTENT_SOURCE_DIR_ASIO=${ASIO_HOME}
			-DDNP3_TLS=${DNP3_TLS}
			-DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}
		CACHE STRING "cmake options to use when building DNP3 submodule"
	)
	if(NOT EXISTS "${DNP3_SOURCE}/.git")
		execute_process(COMMAND git submodule update --init -- Code/submodules/dnp3
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	endif()
	if(NOT EXISTS "${DNP3_BUILD}")
		file(MAKE_DIRECTORY "${DNP3_BUILD}")
	endif()
	if(CMAKE_GENERATOR_PLATFORM)
		set(PLATFORM_OPT "-A${CMAKE_GENERATOR_PLATFORM}")
	elseif(CMAKE_VS_PLATFORM_NAME)
		set(PLATFORM_OPT "-A${CMAKE_VS_PLATFORM_NAME}")
	else()
		set(PLATFORM_OPT "")
	endif()
	message("${CMAKE_COMMAND} ${DNP3_CMAKE_OPTS} -G${CMAKE_GENERATOR} ${PLATFORM_OPT} ${DNP3_SOURCE}")
	execute_process(
		COMMAND ${CMAKE_COMMAND} ${DNP3_CMAKE_OPTS} -G${CMAKE_GENERATOR} ${PLATFORM_OPT} ${DNP3_SOURCE}
		WORKING_DIRECTORY "${DNP3_BUILD}"
		RESULT_VARIABLE EXEC_RESULT
	)
	if(EXEC_RESULT)
		message( FATAL_ERROR "Failed to run cmake for DNP3 submodule, exiting")
	endif()
	set(CONFIGS "${CMAKE_BUILD_TYPE}")
	if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Rel.*")
		list(APPEND CONFIGS "Release")
	endif()
	if("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Deb.*")
		list(APPEND CONFIGS "Debug")
	endif()
	foreach(CONF ${CONFIGS})
		message("Building DNP3 dependency")
		execute_process(
			COMMAND ${CMAKE_COMMAND} --build ${DNP3_BUILD} --config ${CONF} --parallel 8 --target install
			WORKING_DIRECTORY "${DNP3_BUILD}"
			RESULT_VARIABLE EXEC_RESULT
		)
		if(EXEC_RESULT)
			message( FATAL_ERROR "Failed to build DNP3 submodule, exiting")
		endif()
	endforeach()
	add_custom_target( build_dnp3
		WORKING_DIRECTORY "${DNP3_BUILD}"
		COMMAND ${CMAKE_COMMAND} --build ${DNP3_BUILD} --config $<CONFIG> --parallel 8 --target install
	)
else()
	if(DEFINED DNP3_HOME)
		set(DNP3_HOME ${DNP3_HOME} CACHE PATH ${DNP3_HOME_INSTRUCTIONS})
	else()
		set(DNP3_HOME "/usr" CACHE PATH ${DNP3_HOME_INSTRUCTIONS})
	endif()
endif()

#find dnp3 headers
find_path(DNP3_INCLUDE_PATH opendnp3/IStack.h
	PATHS ${DNP3_HOME}/include ${DNP3_HOME}/local/include
	NO_DEFAULT_PATH
	CMAKE_FIND_ROOT_PATH_BOTH)
mark_as_advanced(FORCE DNP3_INCLUDE_PATH)

# Look for regular libraries
find_library(opendnp3_lib NAMES opendnp3 PATHS ${DNP3_HOME}/lib ${DNP3_HOME}/local/lib NO_DEFAULT_PATH CMAKE_FIND_ROOT_PATH_BOTH)
add_library(opendnp3_target UNKNOWN IMPORTED)
set_property(TARGET opendnp3_target PROPERTY IMPORTED_LOCATION "${opendnp3_lib}")
# Look for debug libraries
find_library(opendnp3_dlib NAMES opendnp3${CMAKE_DEBUG_POSTFIX} PATHS ${DNP3_HOME}/lib ${DNP3_HOME}/local/lib NO_DEFAULT_PATH CMAKE_FIND_ROOT_PATH_BOTH)
add_library(opendnp3_dtarget UNKNOWN IMPORTED)
set_property(TARGET opendnp3_dtarget PROPERTY IMPORTED_LOCATION "${opendnp3_dlib}")
#set a variable to use for linking
set(opendnp3_libs debug opendnp3_dtarget optimized opendnp3_target )

#hide (in non-advanced mode) the library paths in cmake guis since they are derrived
mark_as_advanced(FORCE opendnp3_lib opendnp3_dlib)

#include in install/packaging
option(PACKAGE_DNP3 "Package opendnp3 libs in c-pack installer" ON)
if(PACKAGE_DNP3)
	set(PACK_NAMES opendnp3)
endif()

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
if(TARGET build_dnp3)
	add_dependencies(${PROJECT_NAME} build_dnp3)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE "${DNP3_INCLUDE_PATH}")
target_link_libraries(${PROJECT_NAME} ${opendnp3_libs} ODC)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)

install(CODE
"
	set(BUNDLE_DEPS_LIST \${BUNDLE_DEPS_LIST}
		\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_MODULES}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}\${BUNDLE_LIB_POSTFIX}${CMAKE_SHARED_MODULE_SUFFIX}
	)
	set(BUNDLE_DEPS_FILTERIN \${BUNDLE_DEPS_FILTERIN}
		${PACK_NAMES}
	)
")
