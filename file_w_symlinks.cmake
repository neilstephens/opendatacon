#Cmake code to traverse symlink chain and store in list
#code taken from https://stackoverflow.com/a/29708367/6754618 and turned into macro
#thanks swirley161

macro(file_w_symlinks full_chain chain_start_link)

	#Make sure the initial path is absolute.
	get_filename_component(chain_link "${chain_start_link}" ABSOLUTE)

	#Store initial path as first element in list.
	set(${full_chain} "${chain_link}")

	while(UNIX AND IS_SYMLINK "${chain_link}")
		#Grab path to directory containing the current symlink.
		get_filename_component(sym_path "${chain_link}" DIRECTORY)

		#Resolve one level of symlink, store resolved path back in lib.
		execute_process(COMMAND readlink "${chain_link}"
				RESULT_VARIABLE errMsg
				OUTPUT_VARIABLE chain_link
				OUTPUT_STRIP_TRAILING_WHITESPACE)

		#Check to make sure readlink executed correctly.
		if(errMsg AND (NOT "${errMsg}" EQUAL "0"))
			message(FATAL_ERROR "Error calling readlink.")
		endif()

		#Convert resolved path to an absolute path, if it isn't one already.
		if(NOT IS_ABSOLUTE "${chain_link}")
			set(chain_link "${sym_path}/${chain_link}")
		endif()

		#Append resolved path to symlink resolution list.
		list(APPEND ${full_chain} "${chain_link}")
	endwhile()

endmacro()
