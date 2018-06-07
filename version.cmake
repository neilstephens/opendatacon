message("Updating version info")
find_package(Git)
if(GIT_FOUND)
	execute_process(
		COMMAND git describe --long --match "[0-9]*\\.[0-9]*\\.[0-9]" --dirty
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_DESCRIBE
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	message(" Git describe: ${GIT_DESCRIBE}")
	string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)-[0-9]+-([^-]+)-?([^-]*)$" GIT_REPO_MATCH ${GIT_DESCRIBE})
	#message("Matched version: ${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3} ${CMAKE_MATCH_4}")

	set(ODC_MAJOR_VERSION ${CMAKE_MATCH_1})
	set(ODC_MINOR_VERSION ${CMAKE_MATCH_2})
	set(ODC_MICRO_VERSION ${CMAKE_MATCH_3})
	set(ODC_VERSION "${ODC_MAJOR_VERSION}.${ODC_MINOR_VERSION}.${ODC_MICRO_VERSION}")

	set(GIT_REPO_COMMIT ${CMAKE_MATCH_4})
	set(GIT_REPO_DIRTY ${CMAKE_MATCH_5})
	#message("-- opendatacon version: ${ODC_VERSION} ${GIT_REPO_COMMIT} ${GIT_REPO_DIRTY}")

	configure_file (
		"${CMAKE_SOURCE_DIR}/include/opendatacon/Version.h.in"
		"${CMAKE_SOURCE_DIR}/include/opendatacon/Version.h"
	)
	#hack to trigger cmake reconfigure if Version.h changed
	configure_file ("${CMAKE_SOURCE_DIR}/include/opendatacon/Version.h" "${CMAKE_SOURCE_DIR}/include/opendatacon/Version.h" COPYONLY)
endif()
