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

message("Updating version info")
find_package(Git)
if(GIT_FOUND)
	execute_process(
		COMMAND git describe --long --match "[0-9]*\\.[0-9]*\\.[0-9]*" --dirty
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_DESCRIBE
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	message(" Git describe: ${GIT_DESCRIBE}")
	string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(-RC[0-9]+)?-[0-9]+-([^-]+)-?([^-]*)$" GIT_REPO_MATCH ${GIT_DESCRIBE})
	#message("Matched version: ${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3} ${CMAKE_MATCH_4}")

	set(ODC_MAJOR_VERSION ${CMAKE_MATCH_1})
	set(ODC_MINOR_VERSION ${CMAKE_MATCH_2})
	set(ODC_MICRO_VERSION ${CMAKE_MATCH_3})
	set(ODC_RC_VERSION ${CMAKE_MATCH_4})
	set(ODC_VERSION "${ODC_MAJOR_VERSION}.${ODC_MINOR_VERSION}.${ODC_MICRO_VERSION}${ODC_RC_VERSION}")

	set(GIT_REPO_COMMIT ${CMAKE_MATCH_5})
	set(GIT_REPO_DIRTY ${CMAKE_MATCH_6})
	message("-- opendatacon version: ${ODC_VERSION} ${GIT_REPO_COMMIT} ${GIT_REPO_DIRTY}")

	configure_file (
		"${CMAKE_SOURCE_DIR}/include/opendatacon/Version.h.in"
		"${CMAKE_SOURCE_DIR}/include/opendatacon/Version.h"
	)
	#hack to trigger cmake reconfigure if Version.h changed
	configure_file ("${CMAKE_SOURCE_DIR}/include/opendatacon/Version.h" "${CMAKE_SOURCE_DIR}/include/opendatacon/Version.h" COPYONLY)
endif()
