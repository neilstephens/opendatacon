#	opendatacon
 #
 #	Copyright (c) 2019:
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
 # We want to have the KafkaPort include the submodules under the ODC submodule folder, not sub/sub modules. This means we only get one asio and spdlog version.
 # We make zlib and snappy sub modules under ODC as well for completeness.
 # To do this we set the "not submodules" flags for the libkafka-asio sub module
 # This also means that this Cmake file will have some duplication of entries in the libkafka-asio one - possibly..
 # We model it off the dnp3port Cmake file.
 #
 # Asio and Spdlog will be there for all ODC versions, so dont need to worry about them:
 
project(KafkaPort)
cmake_minimum_required(VERSION 3.1)


set(KAFKA_HOME_INSTRUCTIONS "Choose the location of KAFKA header library: where it was installed - there should be an include directory within")

# Do these need to be set to an install directory instead of the source directory? Need to trigger libkafka-asio build in ODC Main makefile at configure...
option(USE_KAFKA_SUBMODULE "Use git submodule to download kakfka header library and sub modules" ON)
option(USE_ZLIB_SUBMODULE "Use git submodule to download zlib code/library" ON)
option(USE_SNAPPY_SUBMODULE "Use git submodule to download snappy code/library" ON)

# We dont want the libkafka-asio module to include all its own sub modules, as some of them are common. 
# Either define them here (zlib/snappy) or use the ones that ODC is already using.

set(KAFKA_USE_ASIO_SUBMODULE OFF)
set(KAFKA_USE_ZLIB_SUBMODULE OFF)
set(KAFKA_USE_SNAPPY_SUBMODULE OFF)
set(KAFKA_USE_SPDLOG_SUBMODULE OFF)

if(USE_KAFKA_SUBMODULE)
	set(KAFKA_HOME "${CMAKE_SOURCE_DIR}/submodules/libkafka-asio" CACHE PATH ${KAFKA_HOME_INSTRUCTIONS} FORCE)

	set(LIBKAFKA_ASIO_BUILD_TESTS "Build the libkafka-asio catch2 based tests" OFF)
	set(LIBKAFKA_ASIO_BUILD_EXAMPLES "Build the libkafka-asio example code" OFF)
	
	set(KAFKA_INCLUDE_DIR "${KAFKA_HOME}/lib")
	
	# The libkafka-asio sub module is include files only, so nothing to build, as long as ZLIB and SNAPPY are dealt with in this CMake file as sub modules.
	if(NOT EXISTS "${KAFKA_HOME}/.git")
		execute_process(COMMAND git submodule update --init -- submodules/libkafka-asio
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	endif()
else()
	if(DEFINED KAFKA_HOME)
		set(KAFKA_HOME ${KAFKA_HOME} CACHE PATH ${KAFKA_HOME_INSTRUCTIONS})
	else()
		set(KAFKA_HOME "/usr" CACHE PATH ${KAFKA_HOME_INSTRUCTIONS})
	endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${KAFKA_HOME}/cmake/Modules")	# Find Snappy Command
include(${KAFKA_HOME}/buildsubmodule.cmake)

# Build or set location of ZLIB
set(ZLIB_HOME_INSTRUCTIONS "Choose the location of ZLIB where it was installed - there should be lib and include directories within")
if(USE_ZLIB_SUBMODULE)
	set(ZLIB_HOME "${CMAKE_BINARY_DIR}/submodules/ZLIB/install" CACHE PATH ${ZLIB_HOME_INSTRUCTIONS} FORCE)
	
	build_submodule(zlib ${ZLIB_HOME} "")
	
else()
	if(DEFINED ZLIB_HOME)
		set(ZLIB_HOME ${ZLIB_HOME} CACHE PATH ${ZLIB_HOME_INSTRUCTIONS})
	else()
		set(ZLIB_HOME "/usr" CACHE PATH ${ZLIB_HOME_INSTRUCTIONS})
	endif()
endif()
message("Zlib_home - ${ZLIB_HOME}")

# Build or set location of SNAPPY
set(SNAPPY_HOME_INSTRUCTIONS "Choose the location of Snappy where it was installed - there should be lib and include directories within")
if(USE_SNAPPY_SUBMODULE)
	set(SNAPPY_HOME "${CMAKE_BINARY_DIR}/submodules/Snappy/install" CACHE PATH ${SNAPPY_HOME_INSTRUCTIONS} FORCE)
	
	build_submodule(snappy ${SNAPPY_HOME} "-DSNAPPY_BUILD_TESTS=OFF;-DHAVE_LIBZ=1")
	
else()
	if(DEFINED SNAPPY_HOME)
		set(SNAPPY_HOME ${SNAPPY_HOME} CACHE PATH ${SNAPPY_HOME_INSTRUCTIONS})
	else()
		set(SNAPPY_HOME "/usr" CACHE PATH ${SNAPPY_HOME_INSTRUCTIONS})
	endif()
endif()
message("Snappy_home - ${SNAPPY_HOME}")

set(Boost_DIR "${KAFKA_HOME}/submodules/boost_1_70_0" CACHE PATH "Set the path to the Boost Subset folder" )

#Have to stuff around to get the correct ZLIB libraries, the find package always seems to come back with the DLL version.
if (BUILD_SHARED_LIBS)
	find_package(ZLIB REQUIRED)
else()
	# Handle appveyor only building the single target.
	set( ZLIB_LIBRARIES "")
	
	if(UNIX)	
		find_library(ZLIB_STATIC libz.a PATHS "${ZLIB_HOME}" NO_SYSTEM_ENVIRONMENT_PATH)	##static linux equivalent.. (.so dynamic). We want to use our version, not one somewhere else in the system
		
		if (NOT (${ZLIB_STATIC} STREQUAL "ZLIB_STATIC-NOTFOUND"))
			list( APPEND ZLIB_LIBRARIES  ${ZLIB_STATIC})
		endif()
	else()
		find_library(ZLIB_STATIC zlibstatic PATHS "${ZLIB_HOME}" NO_SYSTEM_ENVIRONMENT_PATH)	#We want to use our version, not one somewhere else in the system
		find_library(ZLIB_STATICD zlibstaticd PATHS "${ZLIB_HOME}" NO_SYSTEM_ENVIRONMENT_PATH)
	
		if (NOT (${ZLIB_STATIC} STREQUAL "ZLIB_STATIC-NOTFOUND"))
			list( APPEND ZLIB_LIBRARIES optimized ${ZLIB_STATIC})
		endif()
		if (NOT (${ZLIB_STATICD} STREQUAL "ZLIB_STATICD-NOTFOUND"))
			list( APPEND ZLIB_LIBRARIES debug ${ZLIB_STATICD})
		endif()
	endif()
	set(ZLIB_INCLUDE_DIR "${ZLIB_HOME}/include")
endif()

find_package(Boost REQUIRED)
message( "Boost_INCLUDE_DIRS - ${Boost_INCLUDE_DIRS}")

set( SNAPPY_ROOT_DIR ${SNAPPY_HOME} )	# Set the hint variable on where to find it!
find_package(Snappy REQUIRED)

message( "ZLIB_LIBRARIES - ${ZLIB_LIBRARIES}")
message( "SNAPPY_LIBRARIES - ${SNAPPY_LIBRARIES}")
message( "SNAPPY_INCLUDE_DIR - ${SNAPPY_INCLUDE_DIR}")

include_directories(
	"../include"
	"../JSON"
	"${ASIO_HOME}/include"
	"${SPDLOG_HOME}/include"
    "${Boost_INCLUDE_DIRS}"
	"${ZLIB_INCLUDE_DIR}"
	"${SNAPPY_INCLUDE_DIR}"
	"${KAFKA_INCLUDE_DIR}")

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def)

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
target_link_libraries(${PROJECT_NAME} ODC ${ZLIB_LIBRARIES} ${SNAPPY_LIBRARIES} )

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)
