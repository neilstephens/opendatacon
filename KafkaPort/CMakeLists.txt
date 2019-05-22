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
project(KafkaPort)
cmake_minimum_required(VERSION 3.1)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/submodules/libkafka-asio/cmake/Modules")	# Find Snappy Command

# Do these need to be set to an install directory instead of the source directory? Need to trigger libkafka-asio build in ODC Main makefile at configure...

set(Kafka-asio_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/submodules/libkafka-asio/lib" CACHE PATH "Set the path to the kafka-asio lib folder" )
set(ZLIB_HOME "${CMAKE_SOURCE_DIR}/submodules/libkafka-asio/submodules/zlib" CACHE PATH "Set the path to the zlib folder" )
set(SNAPPY_HOME "${CMAKE_SOURCE_DIR}/submodules/libkafka-asio/submodules/snappy" CACHE PATH "Set the path to the snappy folder" )
set(Boost_DIR "${CMAKE_SOURCE_DIR}/submodules/libkafka-asio/submodules/boost_1_70_0" CACHE PATH "Set the path to the Boost Subset folder" )


#Give a pointer on where to find our installed pakages.
list(APPEND CMAKE_PREFIX_PATH "${ZLIB_HOME}")
list(APPEND CMAKE_PREFIX_PATH "${SNAPPY_HOME}")
list(APPEND CMAKE_PREFIX_PATH "${Boost_DIR}")

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
find_package(Snappy REQUIRED)

message( "ZLIB_LIBRARIES - ${ZLIB_LIBRARIES}")
message( "SNAPPY_LIBRARIES - ${SNAPPY_LIBRARIES}")

include_directories(
	"../include"
	"../JSON"
	"${ASIO_HOME}/include"
	"${SPDLOG_HOME}/include"
    "${Boost_INCLUDE_DIRS}"
	"${ZLIB_INCLUDE_DIR}"
	"${SNAPPY_INCLUDE_DIR}"
	"${Kafka-asio_INCLUDE_DIR}")

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def)

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
target_link_libraries(${PROJECT_NAME} ODC ${ZLIB_LIBRARIES} ${SNAPPY_LIBRARIES} )

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)
