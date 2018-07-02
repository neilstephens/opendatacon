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

void* GetPortFunc(const std::string& libname, const std::string& objname, bool destroy)
{
	//Looks for a specific library (for libs that implement more than one class)
	std::string libfilename = GetLibFileName(libname);

	//try to load the lib
	auto pluginlib = LoadModule(libfilename.c_str());

	if (pluginlib == nullptr)
	{
		std::cerr << libname << " Info: dynamic library load failed '" << libfilename << "' :"<< LastSystemError()<< std::endl;
		return nullptr;
	}

	std::string funcname;
	if(destroy)
		funcname = "delete_";
	else
		funcname = "new_";

	funcname += objname + "Port";
	void* port_func = LoadSymbol(pluginlib, funcname.c_str());

	if (port_func == nullptr)
	{
		std::cerr << libname << " Info: failed to load symbol '" << funcname << "' in library '" << libfilename << "' - " << LastSystemError() << std::endl;
		return nullptr;
	}
	return port_func;
}

newptr GetPortCreator(const std::string& libname, const std::string& objname)
{
	return (newptr)GetPortFunc(libname,objname);
}

delptr GetPortDestroyer(const std::string& libname, const std::string& objname)
{
	return (delptr)GetPortFunc(libname,objname,true);
}

