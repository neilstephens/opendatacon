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

//TODO: refactor this to be able to unload the library too

symbol_ptr GetPortFunc(module_ptr pluginlib, const std::string& objname, bool destroy)
{
	std::string funcname;
	if(destroy)
		funcname = "delete_";
	else
		funcname = "new_";

	funcname += objname + "Port";
	symbol_ptr port_func = LoadSymbol(pluginlib, funcname);

	if (port_func == nullptr)
	{
		std::cerr << " Info: failed to load symbol '" << funcname << "' - " << LastSystemError() << std::endl;
		return nullptr;
	}
	return port_func;
}

newptr GetPortCreator(module_ptr pluginlib, const std::string& objname)
{
	return reinterpret_cast<newptr>(GetPortFunc(pluginlib,objname));
}

delptr GetPortDestroyer(module_ptr pluginlib, const std::string& objname)
{
	return reinterpret_cast<delptr>(GetPortFunc(pluginlib,objname,true));
}

