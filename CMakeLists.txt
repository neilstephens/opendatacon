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

option(SUPPRESS_ZERO_CHECK "Turn off the VS ZERO_CHECK config which runs everytime you build" OFF)
if(SUPPRESS_ZERO_CHECK)
	# If you dont want the ZERO_CHECK project in VS have the following line enabled.
	set(CMAKE_SUPPRESS_REGENERATION true)
endif()


if(WIN32)
	cmake_minimum_required(VERSION 3.4)
else()
	cmake_minimum_required(VERSION 2.8)
	set(BUILD_TYPE_INSTRUCTIONS "Choose the type of build, default Release, other options: Debug RelWithDebInfo MinSizeRel.")
	if(DEFINED CMAKE_BUILD_TYPE)
		set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING ${BUILD_TYPE_INSTRUCTIONS})
	else()
		set(CMAKE_BUILD_TYPE Release CACHE STRING ${BUILD_TYPE_INSTRUCTIONS})
	endif()
endif()

if(POLICY CMP0048)
	cmake_policy(SET CMP0048 OLD)
endif()

#macro for resolving chain of symlinks into a list
include(file_w_symlinks.cmake)

project(opendatacon_suite C CXX)
install(FILES "LICENSE.txt" DESTINATION ".")
install(FILES "NOTICE.txt" DESTINATION ".")
install(FILES "README.md" DESTINATION ".")
install(FILES "RELEASE_NOTES" DESTINATION ".")
install(DIRECTORY "./3rdPartyLicenses" DESTINATION ".")
install(DIRECTORY "install/init" DESTINATION ".")
install(DIRECTORY install/systemd DESTINATION ".")

#Deploy git workflow hook scripts
include(git-hooks.cmake)

#This will run on every build, because output is never actually produced.
add_custom_command(OUTPUT always_rebuild COMMAND ${CMAKE_COMMAND} -E echo)

#Generate version info now, and set to generate on every build
include(version.cmake)
add_custom_command(
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT "${CMAKE_SOURCE_DIR}/include/opendatacon/Version.h"
	COMMAND ${CMAKE_COMMAND} -DBINARY_DIR=${CMAKE_BINARY_DIR} -DCURRENT_CONFIG=$<CONFIG> -P "version.cmake"
	DEPENDS always_rebuild
)
#version info target that our other targets can have a dependance on
add_custom_target(version DEPENDS "${CMAKE_SOURCE_DIR}/include/opendatacon/Version.h")

# various optional libraries and projects
option(FULL "Build all optional projects (DNP3Port, JSONPort, ModbusPort, PyPort, SimPort, MD3Port, CBPort, Tests, ConsoleUI, WebUI)" OFF)
option(USE_ASIO_SUBMODULE "Use git submodule to download asio header library" ON)
option(USE_SPDLOG_SUBMODULE "Use git submodule to download spdlog header library" ON)

set(TESTS OFF CACHE BOOL "Build tests")
set(WEBUI OFF CACHE BOOL "Build the http(s) web user interface")
set(DNP3PORT OFF CACHE BOOL "Build DNP3 Port")
set(JSONPORT OFF CACHE BOOL "Build JSON Port")
set(PYPORT OFF CACHE BOOL "Build Python Port")
set(MODBUSPORT OFF CACHE BOOL "Build Modbus Port")
set(SIMPORT OFF CACHE BOOL "Build Simulation Port")
set(MD3PORT OFF CACHE BOOL "Build MD3 Port")
set(CBPORT OFF CACHE BOOL "Build Conitel-Baker Port")
set(CONSOLEUI OFF CACHE BOOL "Build the console user interface")

# other options off-by-default that you can enable
option(WERROR "Set all warnings to errors" OFF)
option(COVERAGE "Builds the libraries with coverage info for gcov" OFF)

if(FULL)
	set(TESTS ON CACHE BOOL "Build tests" FORCE)
	set(WEBUI ON CACHE BOOL "Build the http(s) web user interface" FORCE)
	set(DNP3PORT ON CACHE BOOL "Build DNP3 Port" FORCE)
	set(JSONPORT ON CACHE BOOL "Build JSON Port" FORCE)
	set(PYPORT ON CACHE BOOL "Build Python Port" FORCE)
	set(MODBUSPORT ON CACHE BOOL "Build Modbus Port" FORCE)
	set(SIMPORT ON CACHE BOOL "Build Simulation Port" FORCE)
	set(MD3PORT ON CACHE BOOL "Build MD3 Port" FORCE)
	set(CBPORT ON CACHE BOOL "Build Conitel-Baker Port" FORCE)
	set(CONSOLEUI ON CACHE BOOL "Build the console user interface" FORCE)
endif()

# IDE configuration
set_property(GLOBAL PROPERTY USE_FOLDERS ON) #allows the creation of solution folders

# Platform configuration
if(WIN32)
	# for ASIO
	add_definitions(-D_WIN32_WINNT=0x0502)	
	add_definitions(-DASIO_HAS_STD_SYSTEM_ERROR)
	add_definitions(-DVC_EXTRALEAN)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
	
	# Auto generate def files that export all symbols of STATIC libraries
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
	
	# UNICODE support
	add_definitions(-DUNICODE -D_UNICODE)
	
	# Postfix for debug builds
	SET(CMAKE_DEBUG_POSTFIX "d")
	install(CODE
	"
		if(\"\${CMAKE_INSTALL_CONFIG_NAME}\" STREQUAL \"Debug\")
			set(BUNDLE_LIB_POSTFIX ${CMAKE_DEBUG_POSTFIX})
		endif()
	")
	
	#min max macro rubbish
	add_definitions(-DNOMINMAX)

	# Install directories
	set(INSTALLDIR_BINS ".")
	set(INSTALLDIR_INCLUDES "include")
	set(INSTALLDIR_LIBS "lib")
	set(INSTALLDIR_SHARED ".")
	set(INSTALLDIR_MODULES ".")
endif()
if(UNIX)
	set(DL dl)
	set(RPATH_BASE $ORIGIN)

	# Install directories
	set(INSTALLDIR_BINS "bin")
	set(INSTALLDIR_INCLUDES "include")
	set(INSTALLDIR_LIBS "lib")
	set(INSTALLDIR_SHARED "lib")
	set(INSTALLDIR_MODULES "plugin")
endif()
if(APPLE)
	set(CMAKE_MACOSX_RPATH 1)
	set(RPATH_BASE @executable_path)

	# Install directories
	set(INSTALLDIR_BINS "bin")
	set(INSTALLDIR_INCLUDES "include")
	set(INSTALLDIR_LIBS "lib")
	set(INSTALLDIR_SHARED "lib")
	set(INSTALLDIR_MODULES "plugin")
endif()

# Compiler configuration
add_definitions(-DASIO_STANDALONE) # required for ASIO in C++11 only mode

#uncomment below to debug asio handlers on stderr
#add_definitions(-DASIO_ENABLE_HANDLER_TRACKING)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	# using Visual Studio C++
	message("CMAKE configuring opendatacon for Visual Studio C++" )
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /MP /Zc:rvalueCast")
	# tell asio to only compile once (where <asio/impl/src.hpp> is included)
	# use CXX_FLAGS (instead of add_definitions) so we can use it for external libraries too
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DASIO_SEPARATE_COMPILATION")

elseif(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
	# using Intel C++
	message(WARNING "opendatacon has not been tested using Intel compiler" )
	
else()#if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
	# using Clang or GNU compilers
	message("-- configuring opendatacon for  Clang or GNU compilers" )
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	endif()
	if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		message("using special options for GNU compiler")
		#--no-gnu-unique is required for true dynamic loading and unloading of shared libs
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --no-gnu-unique")
		#now some trickery to see which glibc
		execute_process(
			COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libc.so.6
			OUTPUT_VARIABLE libc_file
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		execute_process(
			COMMAND ${libc_file} --version
			OUTPUT_VARIABLE glibc_version_str
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		if(glibc_version_str)
			message("libc.so.6 --version :")
			message("${glibc_version_str}")
			string(REGEX MATCH "2\\.[0-9][0-9]" glibc_version ${glibc_version_str})
			message("Matched version: ${glibc_version}")
			if(glibc_version LESS 2.17)
				message("linking realtime lib for old libc")
				set(RT "rt")
			endif()
		endif()
	endif()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++17 -pedantic")
	if(NOT APPLE)
		set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-undefined")
		#only export API symbols
	endif()
	# tell asio to only compile once (where <asio/impl/src.hpp> is included)
	# use CXX_FLAGS (instead of add_definitions) so we can use it for external libraries too
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DASIO_SEPARATE_COMPILATION")

	option(STATIC_LIBSTDC++ "Link libstdc++ statically" ON)
	if(STATIC_LIBSTDC++)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++")
		set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libstdc++")
		#why is this not automatic with Clang?
		if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			#need to add these to the link libraries manually
			set(LIBCXX "c++" "c++abi")
		endif()
	endif()

	# different release and debug flags
	set(CMAKE_CXX_FLAGS_RELEASE "-O3")
	set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g")

	if(COVERAGE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
	endif()

	if (WERROR)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
	endif()

	#package the c++ standard library
	option(PACKAGE_LIBSTDC++ "Package libstdc++ in c-pack installer" OFF)
	if(PACKAGE_LIBSTDC++)
		execute_process(
			COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libstdc++.so.6
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
			OUTPUT_VARIABLE std_lib
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		#hide (in non-advanced mode) the library paths in cmake guis since they are derrived
		mark_as_advanced(FORCE std_lib)
		get_filename_component(_resolvedFile "${std_lib}" REALPATH) #check for symolic link
		if(${_resolvedFile} STREQUAL ${std_lib})
			#not a link - just install file
			install(FILES "${std_lib}" DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
		else()
			#is a link -install link and file
			install(FILES "${std_lib}" "${_resolvedFile}" DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
		endif()
	endif()
endif()

option(PACKAGE_SYSTEM_LIBS "Bundle system dependencies into install (to make a standalone package)" OFF)

find_package(Threads REQUIRED)
if(CMAKE_USE_PTHREADS_INIT)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

# the RPATH to be used when installing - allowing local libraries to be found for standalone installs,
# as well as installed locations
SET(CMAKE_INSTALL_RPATH "${RPATH_BASE}"
	"${RPATH_BASE}/${INSTALLDIR_LIBS}"
	"${RPATH_BASE}/${INSTALLDIR_MODULES}"
	"${RPATH_BASE}/../${INSTALLDIR_LIBS}"
	"${RPATH_BASE}/../${INSTALLDIR_MODULES}"
	"${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_LIBS}"
	"${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_MODULES}")

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build" )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build/${INSTALLDIR_LIBS}" )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build" )

# Second, for multi-config builds (e.g. msvc)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
	string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}" )
	set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}" )
	set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}" )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

set(ASIO_HOME_INSTRUCTIONS "Choose the location of asio header library: where it was installed - there should be an include directory within")
if(USE_ASIO_SUBMODULE)
	set(ASIO_HOME "${CMAKE_SOURCE_DIR}/submodules/asio/asio" CACHE PATH ${ASIO_HOME_INSTRUCTIONS} FORCE)
	if(NOT EXISTS "${ASIO_HOME}/../.git")
		execute_process(COMMAND git submodule update --init -- submodules/asio
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	endif()
else()
	if(DEFINED ASIO_HOME)
		set(ASIO_HOME ${ASIO_HOME} CACHE PATH ${ASIO_HOME_INSTRUCTIONS})
	else()
		set(ASIO_HOME "/usr" CACHE PATH ${ASIO_HOME_INSTRUCTIONS})
	endif()
endif()

set(SPDLOG_HOME_INSTRUCTIONS "Choose the location of spdlog header library: where it was installed - there should be an include directory within")
if(USE_SPDLOG_SUBMODULE)
	set(SPDLOG_HOME "${CMAKE_SOURCE_DIR}/submodules/spdlog" CACHE PATH ${SPDLOG_HOME_INSTRUCTIONS} FORCE)
	if(NOT EXISTS "${SPDLOG_HOME}/.git")
		execute_process(COMMAND git submodule update --init -- submodules/spdlog
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	endif()
else()
	if(DEFINED SPDLOG_HOME)
		set(SPDLOG_HOME ${SPDLOG_HOME} CACHE PATH ${SPDLOG_HOME_INSTRUCTIONS})
	else()
		set(SPDLOG_HOME "/usr" CACHE PATH ${SPDLOG_HOME_INSTRUCTIONS})
	endif()
endif()

message("add subdir JSON")
add_subdirectory(JSON)
message("add subdir ODC")
add_subdirectory(ODC)
if(PYPORT OR SIMPORT)
	message("add subdir HTTP")
	add_subdirectory(HTTP)
endif()
message("add subdir opendatacon")
add_subdirectory(opendatacon)
if(DNP3PORT)
	message("add subdir DNP3Port")
	add_subdirectory(DNP3Port)
endif()
if(JSONPORT)
	message("add subdir JSONPort")
	add_subdirectory(JSONPort)
endif()
if(PYPORT)
	message("add subdir PyPort")
	add_subdirectory(PyPort)
endif()
if(WEBUI)
	message("add subdir WebUI")
	add_subdirectory(WebUI)
endif()
if(CONSOLEUI)
	message("add subdir ConsoleUI")
	add_subdirectory(ConsoleUI)
endif()
if(MODBUSPORT)
	message("add subdir ModbusPort")
	add_subdirectory(ModbusPort)
endif()
if(SIMPORT)
	message("add subdir SimPort")
	add_subdirectory(SimPort)
endif()	
if(MD3PORT)
	message("add subdir MD3Port")
	add_subdirectory(MD3Port)
endif()
if(CBPORT)
	message("add subdir CBPort")
	add_subdirectory(CBPort)
endif()
if(TESTS)
	message("add subdir tests")
	enable_testing()
	add_subdirectory(tests)
	add_test(DNP3Port_tests DNP3Port_tests)
	add_test(ODC_tests ODC_tests)
	add_test(MD3_tests MD3_tests)
	add_test(CB_tests CB_tests)
	add_test(Py_tests Py_tests)
endif()
message("add subdir install")
add_subdirectory("install")
