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
cmake_minimum_required (VERSION 2.8)
project (opendatacon)
# The version number.
set (opendatacon_VERSION_MAJOR 0)
set (opendatacon_VERSION_MINOR 2)
set (opendatacon_VERSION_MINOR 3)

# configure a header file to pass some of the CMake settings
# to the source code
#configure_file (
#  "${PROJECT_SOURCE_DIR}/OpenDataConConfig.h.in"
#  "${PROJECT_BINARY_DIR}/OpenDataConConfig.h"
#  )

# add the binary tree to the search path for include files
# so that we will find TutorialConfig.h
include_directories("${PROJECT_BINARY_DIR}")

# Initialize CXXFLAGS.
set(CMAKE_CXX_FLAGS "-Wall -std=c++11")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

if (DEFINED ENV{ASIO_HOME})
  message("ASIO_HOME defined: $ENV{ASIO_HOME}")
  include_directories($ENV{ASIO_HOME})
  set (CMAKE_REQUIRED_INCLUDES $ENV{ASIO_HOME})
endif()

include(CheckIncludeFileCXX)

# Look for and configure asio standalone
CHECK_INCLUDE_FILE_CXX(asio.hpp HAVE_ASIO)
message("Result: ${HAVE_ASIO}")
IF(NOT HAVE_ASIO)
  message(FATAL_ERROR "asio.h not found. You must define ASIO_HOME to point to your ASIO include subdirectory")
ENDIF()
add_definitions(-DASIO_STANDALONE)

include_directories(./include)
include_directories(./ODC)
include_directories(./JSON)

add_definitions(-DASIO_STANDALONE)

file(GLOB_RECURSE ODC_SRC ./ODC/*.cpp)
file(GLOB_RECURSE ODC_INC ./ODC/*.h)
add_library(ODC SHARED ${ODC_SRC} ${ODC_INC})

file(GLOB_RECURSE JSON_SRC ./JSON/*.cpp)
file(GLOB_RECURSE JSON_INC ./JSON/*.h)
add_library(JSON SHARED ${JSON_SRC} ${JSON_INC})

find_library(ASIODNP3_LIBRARY asiodnp3 lib)
find_library(OPENDNP3_LIBRARY opendnp3 lib)
find_library(ASIOPAL_LIBRARY asiopal lib)
find_library(OPENPAL_LIBRARY openpal lib)

file(GLOB ModbusPort_SRC  ./ModbusPort/*.cpp)
file(GLOB ModbusPort_INC  ./ModbusPort/*.h)
file(GLOB LIBMODBUS_SRC  ./ModbusPort/libmodbus/*.c)
file(GLOB LIBMODBUS_INC  ./ModbusPort/libmodbus/*.h)
add_library(ModbusPort SHARED ${ModbusPort_SRC} ${ModbusPort_INC} ${LIBMODBUS_SRC} ${LIBMODBUS_INC})
target_link_libraries(ModbusPort ODC JSON ${ASIODNP3_LIBRARY} ${OPENDNP3_LIBRARY} ${ASIOPAL_LIBRARY} ${OPENPAL_LIBRARY})

file(GLOB_RECURSE DNP3Port_SRC  ./DNP3Port/*.cpp)
file(GLOB_RECURSE DNP3Port_INC  ./DNP3Port/*.h)
add_library(DNP3Port SHARED ${DNP3Port_SRC} ${DNP3Port_INC})
target_link_libraries(DNP3Port ODC JSON ${ASIODNP3_LIBRARY} ${OPENDNP3_LIBRARY} ${ASIOPAL_LIBRARY} ${OPENPAL_LIBRARY})

file(GLOB_RECURSE JSONPort_SRC ./JSONPort/*.cpp)
file(GLOB_RECURSE JSONPort_INC ./JSONPort/*.h)
add_library(JSONPort SHARED ${JSONPort_SRC} ${JSONPort_INC})

#find_library(MICROHTTPD_LIBRARY microhttpd lib)
#file(GLOB_RECURSE WebUI_SRC ./WebUI/*.cpp)
#file(GLOB_RECURSE WebUI_INC ./WebUI/*.h)
#add_library(WebUI SHARED ${WebUI_SRC})
#target_link_libraries(WebUI JSON ${MICROHTTPD_LIBRARY})

target_link_libraries(ODC JSON ${ASIODNP3_LIBRARY} ${OPENDNP3_LIBRARY} ${ASIOPAL_LIBRARY} ${OPENPAL_LIBRARY})
target_link_libraries(JSONPort ODC JSON ${OPENDNP3_LIBRARY} ${OPENPAL_LIBRARY})

file(GLOB_RECURSE opendatacon_SRC ./src/*.cpp)
file(GLOB_RECURSE opendatacon_INC ./src/*.h)
add_executable(opendatacon ${opendatacon_SRC} ${opendatacon_INC})
#set_property(TARGET opendatacon PROPERTY RUNTIME_OUTPUT_DIRECTORY examples)
set_property(TARGET opendatacon PROPERTY WORKING_DIRECTORY example)
target_link_libraries(opendatacon LINK_PUBLIC ODC pthread)
