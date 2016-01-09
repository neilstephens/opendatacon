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
cmake_minimum_required (VERSION 2.8)
 
set(CPACK_PACKAGE_NAME "opendatacon")
set(CPACK_PACKAGE_VENDOR "opendatacon.net")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "opendatacon - Installation")
set(CPACK_PACKAGE_VERSION ${ODC_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${ODC_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${ODC_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${ODC_MICRO_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "opendatacon")

# Mac OSX bundle specific variables
set(CPACK_BUNDLE_NAME opendatacon)
set(CPACK_BUNDLE_PLIST ${CMAKE_SOURCE_DIR}/install/opendatacon.plist)
set(CPACK_BUNDLE_ICON ${CMAKE_SOURCE_DIR}/install/opendatacon.icns)

#RPM Specific variables
#set(CPACK_GENERATOR "RPM")
#set(CPACK_PACKAGE_RELEASE 1)
#set(CPACK_PACKAGE_CONTACT "John Explainer")
#set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
#set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")
include(CPack)
