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
cmake_minimum_required (VERSION 3.21)
 
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

# Use static MSVC runtime to match main project (prevents LNK2038 mismatch)
if(MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_executable(version_stub version.cpp ../Code/Libs/VersionStrings/version.h)
install(TARGETS version_stub RUNTIME DESTINATION ${INSTALLDIR_SHARED})

install(CODE
"
	if(POLICY CMP0207)
		cmake_policy(SET CMP0207 NEW)
	endif()

	# Collect installed targets — only project-built executables and shared
	# libraries.  Skip Python scripts, bundled system libs, and subdirectories.
	set(INSTALLED_TARGETS)
	foreach(DIR ${INSTALLDIR_BINS} ${INSTALLDIR_SHARED} ${INSTALLDIR_MODULES})
		file(GLOB ENTRIES \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/\${DIR}/*\")
		foreach(E \${ENTRIES})
			if(IS_DIRECTORY \${E})
				continue()
			endif()
			get_filename_component(EXT \${E} LAST_EXT)
			if(EXT STREQUAL \".py\")
				continue()
			endif()
			list(APPEND INSTALLED_TARGETS \${E})
		endforeach()
	endforeach()

	# Resolve dependencies per-target (batch call fails with many entries)
	set(RESOLVED_DEPS)
	foreach(TARGET \${INSTALLED_TARGETS})
		file(GET_RUNTIME_DEPENDENCIES
			RESOLVED_DEPENDENCIES_VAR DEPS
			UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED
			EXECUTABLES \${TARGET}
			DIRECTORIES
				\"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_SHARED}\"
				\"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_MODULES}\"
			CONFLICTING_DEPENDENCIES_PRECEDENCE
				/lib /usr/lib /usr/local
			PRE_EXCLUDE_REGEXES
				#linux
				\"^/(lib|lib64)/ld-linux\"
				#windows
				\"[Aa][Pp][Ii]-[Mm][Ss]-[Ww][Ii][Nn]-.*\"
				\"[Ee][Xx][Tt]-[Mm][Ss]-[Ww][Ii][Nn]-.*\"
				\"[Mm][Ss][Vv][Cc][Pp][0-9]+[.][Dd][Ll][Ll]\"
				\"[Vv][Cc][Rr][Uu][Nn][Tt][Ii][Mm][Ee][0-9]+[.][Dd][Ll][Ll]\"
				\"[Uu][Cc][Rr][Tt][Bb][Aa][Ss][Ee][.][Dd][Ll][Ll]\"
				\"[Cc][Oo][Nn][Cc][Rr][Tt][0-9]+[.][Dd][Ll][Ll]\"
				\"[Vv][Cc][Oo][Mm][Pp][0-9]*[.][Dd][Ll][Ll]\"
			POST_EXCLUDE_REGEXES
				#linux
				\"linux-vdso|ld-linux|libc[.]so\"
				#windows
				\"^[A-Za-z]:/[Ww][Ii][Nn][Dd][Oo][Ww][Ss]/\"
				\"/[Ss]ystem32/\"
				\"/[Ss]ys[Ww][Oo][Ww]64/\"
		)
		if(UNRESOLVED)
			message(WARNING \"Unresolved dependencies for \${TARGET}: \${UNRESOLVED}\")
		endif()
		list(APPEND RESOLVED_DEPS \${DEPS})
	endforeach()
	list(REMOVE_DUPLICATES RESOLVED_DEPS)

	if(RESOLVED_DEPS)
		message(STATUS \"Bundling \${RESOLVED_DEPS}\")
		file(INSTALL \${RESOLVED_DEPS} DESTINATION
			\"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_SHARED}\"
			FOLLOW_SYMLINK_CHAIN)
	endif()

	# macOS: fix up dylib install names in every installed target
	if(APPLE)
		set(BUNDLED_LIB_DIR \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${INSTALLDIR_SHARED}\")
		foreach(DYLIB \${RESOLVED_DEPS})
			get_filename_component(DYLIB_NAME \"\${DYLIB}\" NAME)
			execute_process(
				COMMAND install_name_tool -id \"@rpath/\${DYLIB_NAME}\"
					\"\${BUNDLED_LIB_DIR}/\${DYLIB_NAME}\"
				OUTPUT_QUIET ERROR_QUIET
			)
		endforeach()
		foreach(TARGET \${INSTALLED_TARGETS})
			foreach(DYLIB \${RESOLVED_DEPS})
				get_filename_component(DYLIB_NAME \"\${DYLIB}\" NAME)
				execute_process(
					COMMAND install_name_tool -change \"\${DYLIB}\"
						\"@rpath/\${DYLIB_NAME}\"
						\"\${TARGET}\"
					OUTPUT_QUIET ERROR_QUIET
				)
			endforeach()
		endforeach()
	endif()
" COMPONENT Runtime)
