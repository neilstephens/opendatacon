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
		((${CMAKE_MATCH_1} EQUAL 2) AND
		((${CMAKE_MATCH_9} GREATER 9) OR (${CMAKE_MATCH_1} EQUAL 9))))
		message("Setting hooks directory")
		execute_process(
			COMMAND git config core.hooksPath git-hooks
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		)
	else()
		message("Copying hook scripts")
		configure_file ("${CMAKE_SOURCE_DIR}/git-hooks/pre-commit" "${CMAKE_SOURCE_DIR}/.git/hooks/pre-commit" COPYONLY)
	endif()
endif()
