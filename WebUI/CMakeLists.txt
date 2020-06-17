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
project(WebUI)
cmake_minimum_required(VERSION 2.8)
file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def)

set(MICROHTTPD_HOME_INSTRUCTIONS "Choose the location of libmicrohttpd: where it was installed - there should be lib and include directories within")
if(DEFINED MICROHTTPD_HOME)
	set(MICROHTTPD_HOME ${MICROHTTPD_HOME} CACHE PATH ${MICROHTTPD_HOME_INSTRUCTIONS})
else()
	set(MICROHTTPD_HOME "/usr" CACHE PATH ${MICROHTTPD_HOME_INSTRUCTIONS})
endif()

#find microhttpd headers
find_path(HTTPD_INCLUDE_PATH microhttpd.h
	PATHS ${MICROHTTPD_HOME}/include ${MICROHTTPD_HOME}/local/include
	NO_DEFAULT_PATH
	CMAKE_FIND_ROOT_PATH_BOTH)
mark_as_advanced(FORCE HTTPD_INCLUDE_PATH)

#import the microhttpd lib
find_library(HTTPD_LIB NAMES microhttpd libmicrohttpd
	PATHS ${MICROHTTPD_HOME}/lib ${MICROHTTPD_HOME}/local/lib
	PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE}
	NO_DEFAULT_PATH
	CMAKE_FIND_ROOT_PATH_BOTH)
add_library(httpd_target UNKNOWN IMPORTED)
set_property(TARGET httpd_target PROPERTY IMPORTED_LOCATION "${HTTPD_LIB}")

#hide (in non-advanced mode) the library path in cmake guis since it's derrived
mark_as_advanced(FORCE HTTPD_LIB)

#include in install/packaging
option(PACKAGE_LIBMICROHTTPD "Package libmicrohttpd in c-pack installer" ON)
if(PACKAGE_LIBMICROHTTPD)
	set(PACK_NAMES microhttpd libmicrohttpd gcrypt gnutls gpg)
	if(WIN32)
		get_filename_component(HTTPD_LIB_DIR ${HTTPD_LIB} DIRECTORY)
		set(HTTP_DLL_DIR ${MICROHTTPD_HOME}/bin)
	endif()
endif()

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE "${HTTPD_INCLUDE_PATH}")
target_link_libraries(${PROJECT_NAME} ODC httpd_target)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER plugins)

install(DIRECTORY "www" DESTINATION ".")

install(CODE
"
	set(BUNDLE_DEPS_LIST \${BUNDLE_DEPS_LIST}
		\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_MODULES}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}\${BUNDLE_LIB_POSTFIX}${CMAKE_SHARED_MODULE_SUFFIX}
	)
	set(BUNDLE_DEPS_FILTERIN \${BUNDLE_DEPS_FILTERIN}
		${PACK_NAMES}
	)
	set(BUNDLE_DEPS_DIRS \${BUNDLE_DEPS_DIRS}
		${HTTP_DLL_DIR}
	)
")
