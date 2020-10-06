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

message("Deploying git hooks")
find_package(Git)
if(GIT_FOUND)
	execute_process(
		COMMAND git --version
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	#message(" Git version: ${GIT_VERSION}")
	string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" GIT_VER_MATCH ${GIT_VERSION})
	#message("Matched version: ${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3}")

	#git version 2.9 can use a different hooks location
	if((${CMAKE_MATCH_1} GREATER 2) OR
		((${CMAKE_MATCH_1} EQUAL 2) AND (${CMAKE_MATCH_2} GREATER 8)))
		message("Setting hooks directory")
		execute_process(
			COMMAND git config core.hooksPath Code/scripts/git-hooks
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		)
	else()
		message("Copying hook scripts")
		configure_file ("${CMAKE_SOURCE_DIR}/Code/scripts/git-hooks/pre-commit" "${CMAKE_SOURCE_DIR}/.git/hooks/pre-commit" COPYONLY)
	endif()

	#git version 2.14 can keep submodules updated
	if((${CMAKE_MATCH_1} GREATER 2) OR
		((${CMAKE_MATCH_1} EQUAL 2) AND (${CMAKE_MATCH_2} GREATER 13)))
		message("Setting submodule recurse")
		execute_process(
			COMMAND git config submodule.recurse true
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		)
	endif()

endif()
