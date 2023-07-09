/*	opendatacon
 *
 *	Copyright (c) 2014:
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
/*
 * DynamicSymbols.h
 *
 *  Created on: 9/7/2023
 *      Author: Neil Stephens
 */
#ifndef DYNAMICSYMBOLS_H
#define DYNAMICSYMBOLS_H

#include <opendatacon/Platform.h>

namespace Lua
{
//class to ensure the lua symbols are dynamically loaded and added to the global symbol table
//	The lua plugins in opendatacon need this, even though they're linked to the lua shared lib:
//		to enable Lua C modules, that are 'required', to access the symbols
class DynamicSymbols
{
public:
	DynamicSymbols()
	{
		auto libname = GetLibFileName("Lua");
		constexpr bool global = true;
		p = LoadModule(libname,global);
	}
	~DynamicSymbols()
	{
		UnLoadModule(p);
	}
private:
	module_ptr p;
};

} //namespace Lua

#endif // DYNAMICSYMBOLS_H
