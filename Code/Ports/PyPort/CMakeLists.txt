#	opendatacon
 #
 #	Copyright (c) 2017:
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
project(PyPort)

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def server/*.cpp server/*.hpp PythonCode/*.py)

option(USE_PYTHON_SUBMODULE "Use git submodule to download and build python libraries" OFF)
set(PYTHON_HOME_INSTRUCTIONS "Choose the location of Python: where it was installed - there should be lib and include directories within")
include("compile-python.cmake")
include("system-python.cmake")

#hide (in non-advanced mode) the library/include path in cmake guis since it's derrived
mark_as_advanced(FORCE PYTHON_INCLUDE_DIRS)
mark_as_advanced(FORCE PYTHON_LIBRARY_DEBUG)
mark_as_advanced(FORCE PYTHON_LIBRARY_RELEASE)
mark_as_advanced(FORCE PYTHON_LIBRARIES)

foreach(lib_file ${PYTHON_LIBRARY_DEBUG} ${PYTHON_LIBRARY_RELEASE})
	get_filename_component(lib_dir ${lib_file} DIRECTORY)
	set(PYTHON_LIBRARY_DIRS ${PYTHON_LIBRARY_DIRS} ${lib_dir})
endforeach()

message("Python Include Dirs ${PYTHON_INCLUDE_DIRS}")
message("Python Library Dirs ${PYTHON_LIBRARY_DIRS}")
message("Python Debug Library ${PYTHON_LIBRARY_DEBUG} ")
message("Python Release Library ${PYTHON_LIBRARY_RELEASE} ")
message("Python link target string '${PYTHON_LIBRARIES}'")

if("${CMAKE_CONFIGURATION_TYPES}" STREQUAL "")
	configure_file(PythonCode/PyPortSim.py ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/PyPortSim.py COPYONLY )
else()
	foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
		string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
		configure_file(PythonCode/PyPortSim.py ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}}/PyPortSim.py COPYONLY )
	endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )
endif()

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE "${PYTHON_INCLUDE_DIRS}")
target_link_libraries(${PROJECT_NAME} ODC HTTP ${PYTHON_LIBRARIES})

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)

# So tests will run
install(FILES PythonCode/PyPortSim.py DESTINATION ${INSTALLDIR_BINS})

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
	if(PYTHON_EXE_NAMES)
		set(BUNDLE_IGNORE_ITEMS \${BUNDLE_IGNORE_ITEMS}
			${PYTHON_EXE_NAMES}
		)
	endif()
	if(PYTHON_HOME)
		set(BUNDLE_DEPS_DIRS \${BUNDLE_DEPS_DIRS}
			${PYTHON_LIBRARY_DIRS} ${PYTHON_HOME}
		)
	endif()
")

