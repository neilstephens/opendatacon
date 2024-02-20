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
project (opendatacon_installer)
cmake_minimum_required (VERSION 3.5)
 
set(CPACK_PACKAGE_NAME "opendatacon")
set(CPACK_PACKAGE_VENDOR "opendatacon.net")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "opendatacon - Installation")
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/RELEASE_NOTES")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE_HEADER.txt")
set(CPACK_PACKAGE_VERSION ${ODC_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${ODC_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${ODC_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${ODC_MICRO_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})

set(BUILD_ARCH ${CMAKE_SYSTEM_PROCESSOR})

if(CMAKE_GENERATOR_PLATFORM)
	set(BUILD_ARCH ${CMAKE_GENERATOR_PLATFORM})
elseif(CMAKE_VS_PLATFORM_NAME)
	set(BUILD_ARCH ${CMAKE_VS_PLATFORM_NAME})
elseif(CMAKE_GENERATOR MATCHES "^Visual Studio(.*)$")
	if(CMAKE_MATCH_1 MATCHES "^.*Win64$")
		set(BUILD_ARCH "x64")
	else()
		set(BUILD_ARCH "x86")
	endif()
endif()

if(ARCH_DESC)
	set(BUILD_ARCH ${ARCH_DESC})
endif()

if(EXISTS "${CMAKE_BINARY_DIR}/current_build_config")
	message("Reading build config from temp file ${CMAKE_BINARY_DIR}/current_build_config")
	file(READ "${CMAKE_BINARY_DIR}/current_build_config" build_type)
	message("Got '${build_type}'")
elseif(CMAKE_BUILD_TYPE)
	message("Using CMAKE_BUILD_TYPE '-${CMAKE_BUILD_TYPE}'")
	set(build_type "-${CMAKE_BUILD_TYPE}")
else()
	message("Unknown build type...")
	set(build_type "")
endif()

set(CPACK_PACKAGE_FILE_NAME "opendatacon-${GIT_DESCRIBE}-${CMAKE_SYSTEM_NAME}-${BUILD_ARCH}-${CMAKE_SYSTEM_VERSION}${build_type}")
message("CPack package filename base: ${CPACK_PACKAGE_FILE_NAME}")

# Mac OSX bundle specific variables
set(CPACK_BUNDLE_NAME opendatacon)
set(CPACK_BUNDLE_PLIST ${CMAKE_SOURCE_DIR}/install/MacOS/opendatacon.plist)
set(CPACK_BUNDLE_ICON ${CMAKE_SOURCE_DIR}/install/MacOS/AplicationIcon.icns)

#RPM Specific variables
#set(CPACK_GENERATOR "RPM")
#set(CPACK_PACKAGE_RELEASE 1)
#set(CPACK_PACKAGE_CONTACT "opendatacon.net")
#set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
#set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")
include(CPack)

add_executable(version_stub version.cpp ../Code/Libs/VersionStrings/version.h)
target_link_libraries(version_stub ${LIBCXX})
install(TARGETS version_stub RUNTIME DESTINATION ${INSTALLDIR_SHARED})

if(PACKAGE_SYSTEM_LIBS)
	set(BUNDLE_OVERRIDE
	"
		function(gp_resolved_file_type_override resolved_file type_var)
			foreach(filter \${BUNDLE_DEPS_FILTEROUT})
				if(resolved_file MATCHES \"\${filter}\")
					set(\${type_var} system PARENT_SCOPE)
				else()
					set(\${type_var} local PARENT_SCOPE)
				endif()
			endforeach()
		endfunction()
	")
else()
	set(BUNDLE_OVERRIDE
	"
		if(BUNDLE_DEPS_FILTERIN)
			function(gp_resolved_file_type_override resolved_file type_var)
				if(resolved_file MATCHES \"^${CMAKE_FIND_ROOT_PATH}/?(usr/)?lib\")
					set(\${type_var} system PARENT_SCOPE)
				endif()
				foreach(filter \${BUNDLE_DEPS_FILTERIN})
					if(resolved_file MATCHES \"\${filter}\")
						set(\${type_var} local PARENT_SCOPE)
					endif()
				endforeach()
				foreach(filter \${BUNDLE_DEPS_FILTEROUT})
					if(resolved_file MATCHES \"\${filter}\")
						set(\${type_var} system PARENT_SCOPE)
					endif()
				endforeach()
			endfunction()
		else()
			unset(BUNDLE_DEPS_LIST)
		endif()
	")
endif()

install(CODE
"
	${BUNDLE_OVERRIDE}

	if(BUNDLE_DEPS_LIST)
		include(BundleUtilities)

		foreach(dep \${BUNDLE_DEPS_LIST})
			LIST_PREREQUISITES(\${dep} 1 0 1)
		endforeach()

		fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_SHARED}/version_stub${CMAKE_EXECUTABLE_SUFFIX}\" \"\${BUNDLE_DEPS_LIST}\" \"\${BUNDLE_DEPS_DIRS}\" IGNORE_ITEM \"\${BUNDLE_IGNORE_ITEMS}\")
	endif()
" COMPONENT Runtime)
