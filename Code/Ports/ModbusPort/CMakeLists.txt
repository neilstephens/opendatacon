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
project(ModbusPort)

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def)

set(MODBUS_HOME_INSTRUCTIONS "Choose the location of libmodbus: where it was installed - there should be lib and include directories within")
if(DEFINED MODBUS_HOME)
	set(MODBUS_HOME ${MODBUS_HOME} CACHE PATH ${MODBUS_HOME_INSTRUCTIONS})
else()
	set(MODBUS_HOME "/usr" CACHE PATH ${MODBUS_HOME_INSTRUCTIONS})
endif()

#find modbus headers
find_path(MODBUS_INCLUDE_PATH modbus/modbus.h
	PATHS ${MODBUS_HOME}/include ${MODBUS_HOME}/local/include
	NO_DEFAULT_PATH
	CMAKE_FIND_ROOT_PATH_BOTH)
mark_as_advanced(FORCE MODBUS_INCLUDE_PATH)

#find modbus library
find_library(MODBUS_LIB NAMES modbus libmodbus
	PATHS ${MODBUS_HOME}/lib ${MODBUS_HOME}/local/lib
	PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE}
	NO_DEFAULT_PATH
	CMAKE_FIND_ROOT_PATH_BOTH)
add_library(modbus_target UNKNOWN IMPORTED)
set_property(TARGET modbus_target PROPERTY IMPORTED_LOCATION "${MODBUS_LIB}")

#hide (in non-advanced mode) the library path in cmake guis since it's derrived
mark_as_advanced(FORCE MODBUS_LIB)

#include in install/packaging
option(PACKAGE_LIBMODBUS "Package libmodbus in c-pack installer" ON)
if(PACKAGE_LIBMODBUS)
	set(PACK_NAMES modbus libmodbus)
	if(WIN32)
		get_filename_component(MODBUS_DLL_DIR ${MODBUS_LIB} DIRECTORY)
	endif()
endif()

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE "${MODBUS_INCLUDE_PATH}")
target_link_libraries(${PROJECT_NAME} ODC modbus_target)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)

install(CODE
"
	set(BUNDLE_DEPS_LIST \${BUNDLE_DEPS_LIST}
		\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_MODULES}/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}\${BUNDLE_LIB_POSTFIX}${CMAKE_SHARED_MODULE_SUFFIX}
	)
	if(PACK_NAMES)
		set(BUNDLE_DEPS_FILTERIN \${BUNDLE_DEPS_FILTERIN}
			${PACK_NAMES}
		)
	endif()
	if(MODBUS_DLL_DIR)
		set(BUNDLE_DEPS_DIRS \${BUNDLE_DEPS_DIRS}
			${MODBUS_DLL_DIR}
		)
	endif()
")
