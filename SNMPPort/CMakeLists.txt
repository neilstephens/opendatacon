 #	opendatacon
 #
 #	Copyright (c) 2016:
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
project(SNMPPort)
cmake_minimum_required(VERSION 2.8)
file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def)

include_directories(
	../include
	../JSON
	${DNP3_HOME}/include
	${ASIO_HOME}/include
	${SNMP_HOME}/include
)

add_definitions(-DSTDCXX_98_HEADERS=true)

#find SNMP++ library
find_library(SNMP_LIB NAMES snmp++ libsnmp++
	PATHS ${SNMP_HOME}/lib)
add_library(snmp_target UNKNOWN IMPORTED)
set_property(TARGET snmp_target PROPERTY IMPORTED_LOCATION "${SNMP_LIB}")
#include in install/packaging
get_filename_component(_resolvedFile “"{SNMP_LIB}" REALPATH) #check for symbolic link
if(${_resolvedFile} STREQUAL ${SNMP_LIB})
	#not a link - just install file
	install(FILES "${SNMP_LIB}" DESTINATION ${INSTALLDIR_LIBS})
else()
	#is a link -install link and file
	install(FILES "${SNMP_LIB}" "${_resolvedFile}" DESTINATION ${INSTALLDIR_LIBS})
endif()

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
target_link_libraries(${PROJECT_NAME} ODC snmp_target ${openpal_libs} ${opendnp3_libs})

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)
