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
cmake_minimum_required(VERSION 2.8)
file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h *.def server/*.cpp server/*.hpp PythonCode/*.py)

# For Appveyor the paths are C:\Python37 for x86 and C:\Python37-x64 for 3.7.3 which is what we are using. 
# For Appveyor we need to set Python_ROOT_DIR so it finds the correct version x86 or x64. There is code in the appveyor.yml file to do this.
# For travis cross compilation, uninstall Python3 from the host, so that the find (below) will find the target include and library files.
# The travis files include some commands to create symbolic links for python platfomr specific python.h files.
#
find_package(Python3 COMPONENTS Development)	# The new way of doing things, can pass Python_ROOT_DIR as a hint

if (Python3_FOUND) 
	message("Python3 Found")

	if(WIN32)
		if (NOT Python3_LIBRARY_DEBUG)
			# Appveyor does not have the debug libraries. Point to our copy that we have put into GIT. This issue will be fixed on next Image Release
			# Remove this and Python37_d.lib from GIT when the Appveyor image catches up.

			if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")	# 64 Bit
				set(platform "x64")
			else()
				set(platform "x86")
			endif()

			message("Found Python Release Library but no Debug Library ${Python3_LIBRARY_RELEASE}")
			get_filename_component(LibFileName "${Python3_LIBRARY_RELEASE}" NAME_WE)
			message("Target Filename: ${CMAKE_SOURCE_DIR}/${LibFileName}_d.lib")
			set(Python3_LIBRARIES "optimized;${Python3_LIBRARY_RELEASE};debug;${CMAKE_SOURCE_DIR}/PyPort/${platform}/${LibFileName}_d.lib")
		endif()
	endif()
else()
	# We have to find the libraries using old CMake command as Python3 does not work on some of our targets.
	message("find_package(Python3) failed, trying find_package(PythonLibs) ")
	find_package(PythonLibs 3.4 REQUIRED)
	set(Python3_INCLUDE_DIRS "${PYTHON_INCLUDE_DIRS}")
	set(Python3_LIBRARIES "${PYTHON_LIBRARIES}")
endif()


message("Architecture ${CMAKE_LIBRARY_ARCHITECTURE} ")
message("Found Python Include Dirs ${Python3_INCLUDE_DIRS}")
message("Found Python Libraries ${Python3_LIBRARIES} ")
message("Make Bin Dir ${CMAKE_CURRENT_BINARY_DIR}, Install Bin Dir ${INSTALLDIR_BINS}")
#hide (in non-advanced mode) the library/include path in cmake guis since it's derrived
mark_as_advanced(FORCE Python3_INCLUDE_DIRS)
mark_as_advanced(FORCE Python3_LIBRARIES)

add_library(python_target UNKNOWN IMPORTED)
set_property(TARGET python_target PROPERTY IMPORTED_LOCATION "${Python3_LIBRARIES}")

configure_file(PythonCode/PyPortSim.py ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/PyPortSim.py COPYONLY )

#include in install/packaging
option(PACKAGE_LIBPYTHON "Package Python libs in c-pack installer" ON)
if(PACKAGE_LIBPYTHON)
	file_w_symlinks(file_w_links "${Python3_LIBRARIES}")
	install(FILES ${file_w_links} DESTINATION ${INSTALLDIR_LIBS})
	if(WIN32)
		get_filename_component(PYTHON_DIR ${Python3_LIBRARIES} DIRECTORY)
		get_filename_component(PYTHON_BASE ${Python3_LIBRARIES} NAME_WE)
		install(FILES "${PYTHON_DIR}/${PYTHON_BASE}.dll" DESTINATION ${INSTALLDIR_LIBS})
	endif()
endif()

add_library(${PROJECT_NAME} MODULE ${${PROJECT_NAME}_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE "${Python3_INCLUDE_DIRS}")
target_link_libraries(${PROJECT_NAME} ODC ${Python3_LIBRARIES})

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALLDIR_MODULES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ports)

# So tests will run
install(FILES PythonCode/PyPortSim.py DESTINATION ${INSTALLDIR_BINS})
