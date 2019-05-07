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
cmake_minimum_required(VERSION 2.8)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules")

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def)

find_package(Boost REQUIRED) 
find_package(ZLIB REQUIRED)
find_package(Snappy REQUIRED)

set(Kafka-asio_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/submodules/libkafka-asio/lib" CACHE PATH "Set the path to the kafka-asio lib folder" )

include_directories(
	"../include"
	"../JSON"
	"${ASIO_HOME}/include"
	"${SPDLOG_HOME}/include"
    "${Boost_INCLUDE_DIRS}"
	"${ZLIB_INCLUDE_DIR}"
	"${SNAPPY_INCLUDE_DIR}"
	"${Kafka-asio_INCLUDE_DIR}")

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
target_link_libraries(${PROJECT_NAME} ODC ${ZLIB_LIBRARIES} ${SNAPPY_LIBRARIES} )

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)
