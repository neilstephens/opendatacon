if(NOT USE_PYTHON_SUBMODULE)
	# For Appveyor the paths are C:\Python37 for x86 and C:\Python37-x64 for 3.7.3 which is what we are using.
	# For Appveyor we need to set Python_ROOT_DIR so it finds the correct version x86 or x64. There is code in the appveyor.yml file to do this.
	# For travis cross compilation, uninstall Python3 from the host, so that the find (below) will find the target include and library files.
	# The travis files include some commands to create symbolic links for python platfomr specific python.h files.
	#
	find_package(Python3 COMPONENTS Development)	# The new way of doing things, can pass Python_ROOT_DIR as a hint

	if (Python3_FOUND)
		message("Python3 Found")
	else()
		# We have to find the libraries using old CMake command as Python3 does not work on some of our targets.
		message("find_package(Python3) failed, trying find_package(PythonLibs) ")
		find_package(PythonLibs 3.4 REQUIRED)
		if(PYTHONLIBS_FOUND)
			message("Python3 Found")
		else()
			message(FATAL_ERROR "Can't find Python3 - bailing")
		endif()
		set(Python3_INCLUDE_DIRS "${PYTHON_INCLUDE_DIRS}")
		set(Python3_LIBRARIES "${PYTHON_LIBRARIES}")
		set(Python3_LIBRARY_DEBUG "${PYTHON_LIBRARY_DEBUG}")
		set(Python3_LIBRARY_RELEASE "${PYTHON_LIBRARY_RELEASE}")
	endif()
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
			set(Python3_LIBRARY_DEBUG "${CMAKE_SOURCE_DIR}/PyPort/${platform}/${LibFileName}_d.lib")
			set(Python3_RUNTIME_LIBRARY_DEBUG "${CMAKE_SOURCE_DIR}/PyPort/${platform}/${LibFileName}_d.dll")
			set(Python3_LIBRARIES "optimized;${Python3_LIBRARY_RELEASE};debug;${Python3_LIBRARY_DEBUG}")
		endif()
	else()
		if (NOT Python3_LIBRARY_DEBUG)
			set(Python3_LIBRARY_DEBUG "${Python3_LIBRARY_RELEASE}")
		endif()
	endif()

	add_library(python_target UNKNOWN IMPORTED)
	set_property(TARGET python_target PROPERTY IMPORTED_LOCATION "${Python3_LIBRARY_RELEASE}")
	add_library(python_dtarget UNKNOWN IMPORTED)
	set_property(TARGET python_dtarget PROPERTY IMPORTED_LOCATION "${Python3_LIBRARY_DEBUG}")

	set(PYTHON_INCLUDE_DIRS "${Python3_INCLUDE_DIRS}")
	set(PYTHON_LIBRARIES "${Python3_LIBRARIES}")
	set(PYTHON_LIBRARY_DEBUG "${Python3_LIBRARY_DEBUG}")
	set(PYTHON_LIBRARY_RELEASE "${Python3_LIBRARY_RELEASE}")

	#hide (in non-advanced mode) the library/include path in cmake guis since it's derrived
	mark_as_advanced(FORCE Python3_INCLUDE_DIRS)
	mark_as_advanced(FORCE Python3_LIBRARIES)
	mark_as_advanced(FORCE Python3_LIBRARY_DEBUG)
	mark_as_advanced(FORCE Python3_LIBRARY_RELEASE)
endif()
