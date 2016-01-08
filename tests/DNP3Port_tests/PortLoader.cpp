/*	opendatacon
*
*	Copyright (c) 2015:
*
*		DCrip3fJguWgVCLrZFfA7sIGgvx1Ou3fHfCxnrz4svAi
*		yxeOtDhDCXf1Z4ApgXvX5ahqQmzRfJ2DoX8S05SqHA==
*
*	Licensed under the Apache License, Version 2.0 (the "License");
*	you may not use this file except in compliance with the License.
*	You may obtain a copy of the License at
*
*		http://www.apache.org/licenses/LICENSE-2.0
*
*	Unless required by applicable law or agreed to in writing, software
*	distributed under the License is distributed on an "AS IS" BASIS,
*	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*	See the License for the specific language governing permissions and
*	limitations under the License.
*/
/**
*/

#include "PortLoader.h"

fptr GetPortCreator(std::string libname, std::string objname)
{
	//Looks for a specific library (for libs that implement more than one class)
	std::string libfilename = GetLibFileName(libname);

	//try to load the lib
	auto* pluginlib = LoadModule(libfilename.c_str());

	if (pluginlib == nullptr)
	{
		std::cout << libname << " Info: dynamic library load failed '" << libfilename << std::endl;
		return nullptr;
	}

	//Our API says the library should export a creation function: DataPort* new_<Type>Port(Name, Filename, Overrides)
	//it should return a pointer to a heap allocated instance of a descendant of DataPort
	std::string new_funcname = "new_" + objname + "Port";
	fptr new_plugin_func = (fptr)LoadSymbol(pluginlib, new_funcname.c_str());

	if (new_plugin_func == nullptr)
	{
		std::cout << libname << " Info: failed to load symbol '" << new_funcname << "' in library '" << libfilename << "' - " << LastSystemError() << std::endl;
		std::cout << libname << " Error: failed to load plugin, skipping..." << std::endl;
		return nullptr;
	}
	return new_plugin_func;
}