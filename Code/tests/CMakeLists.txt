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
add_subdirectory(ODC_tests)

if(DNP3PORT)
add_subdirectory(DNP3Port_tests)
endif()

if(MD3PORT)
add_subdirectory(MD3Port_tests)
endif()

if(CBPORT)
add_subdirectory(CBPort_tests)
endif()

if(PYPORT)
add_subdirectory(PyPort_tests)
endif()

if(SIMPORT)
add_subdirectory(SimPort_tests)
endif()

if(FILETRANSFERPORT)
add_subdirectory(FileTransferPort_tests)
endif()

if(LUAPORT OR LUATRANSFORM OR LUALOGSINK OR LUAUICOMMANDER)
add_subdirectory(libLua_tests)
endif()

if(LUAPORT)
add_subdirectory(LuaPort_tests)
endif()

if(LUAWEBPORT)
add_subdirectory(LuaWebPort_tests)
endif()

if(FULL)
add_subdirectory(Integration_tests)
endif()
