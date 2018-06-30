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
cmake_minimum_required(VERSION 2.8)
file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def)


# Locate opendnp3
set(DNP3_HOME_INSTRUCTIONS "Choose the location of opendnp3: where it was installed - there should be lib and include directories within")
if(DEFINED DNP3_HOME)
	set(DNP3_HOME ${DNP3_HOME} CACHE PATH ${DNP3_HOME_INSTRUCTIONS})
else()
	set(DNP3_HOME "/usr/local" CACHE PATH ${DNP3_HOME_INSTRUCTIONS})
endif()
foreach( component openpal opendnp3 asiopal asiodnp3 )
	# Look for regular libraries
	find_library(${component}_lib NAMES ${component} PATHS ${DNP3_HOME}/lib NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
	add_library(${component}_target UNKNOWN IMPORTED)
	set_property(TARGET ${component}_target PROPERTY IMPORTED_LOCATION "${${component}_lib}")
	# Look for debug libraries
	find_library(${component}_dlib NAMES ${component}${CMAKE_DEBUG_POSTFIX} PATHS ${DNP3_HOME}/lib NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
	add_library(${component}_dtarget UNKNOWN IMPORTED)
	set_property(TARGET ${component}_dtarget PROPERTY IMPORTED_LOCATION "${${component}_dlib}")
	#set a variable to use for linking
	set(${component}_libs debug ${component}_dtarget optimized ${component}_target )

	#hide (in non-advanced mode) the library paths in cmake guis since they are derrived
	mark_as_advanced(FORCE ${component}_lib ${component}_dlib)

	#include in install/packaging
	get_filename_component(_resolvedFile "${${component}_lib}" REALPATH) #check for symolic link
	if(${_resolvedFile} STREQUAL ${${component}_lib})
		#not a link - just install file
		install(FILES "${${component}_lib}" DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Release" "RelWithDebInfo" "MinSizeRel")
	else()
		#is a link -install link and file
		install(FILES "${${component}_lib}" "${_resolvedFile}" DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Release" "RelWithDebInfo" "MinSizeRel")
	endif()
	get_filename_component(_resolvedFile "${${component}_dlib}" REALPATH) #check for symolic link
	if(${_resolvedFile} STREQUAL ${${component}_dlib})
		#not a link - just install file
		install(FILES "${${component}_dlib}" DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Debug")
	else()
		#is a link -install link and file
		install(FILES "${${component}_dlib}" "${_resolvedFile}" DESTINATION ${INSTALLDIR_LIBS} CONFIGURATIONS "Debug")
	endif()
endforeach()


include_directories(
	../include
	../JSON
	${SPDLOG_HOME}/include
	${DNP3_HOME}/include
	${ASIO_HOME}/include
)

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
target_link_libraries(${PROJECT_NAME} ODC ${asiodnp3_libs} ${asiopal_libs} ${opendnp3_libs} ${openpal_libs})

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)
