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
 *	AppIINFlags.h
 *
 *  Created on: 2021-04-18
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef APPIINFLAGS_H
#define APPIINFLAGS_H

#include <opendatacon/EnumClassFlags.h>
#include <string>

namespace odc
{

enum class AppIINFlags: uint8_t
{
	NONE                  = 0,
	NEED_TIME             = 1<<0,
	LOCAL_CONTROL         = 1<<1,
	DEVICE_TROUBLE        = 1<<2,
	CONFIG_CORRUPT        = 1<<3,
	EVENT_BUFFER_OVERFLOW = 1<<4
};
ENABLE_BITWISE(AppIINFlags)

inline std::string ToString(const AppIINFlags q)
{
#define IINFLAGSTRING(E,X) if((q & E::X) == E::X) s += #X "|";
	std::string s = "|";
	IINFLAGSTRING(AppIINFlags,NEED_TIME            )
	IINFLAGSTRING(AppIINFlags,LOCAL_CONTROL        )
	IINFLAGSTRING(AppIINFlags,DEVICE_TROUBLE       )
	IINFLAGSTRING(AppIINFlags,CONFIG_CORRUPT       )
	IINFLAGSTRING(AppIINFlags,EVENT_BUFFER_OVERFLOW)
	if(s == "|") return "|NONE|";
	return s;
}

inline AppIINFlags FromString(const std::string& str)
{
#define CHECKIINFLAGSTRING(X) if (str.find(#X) != std::string::npos) flags |= AppIINFlags::X
	AppIINFlags flags = AppIINFlags::NONE;
	CHECKIINFLAGSTRING(NEED_TIME            );
	CHECKIINFLAGSTRING(LOCAL_CONTROL        );
	CHECKIINFLAGSTRING(DEVICE_TROUBLE       );
	CHECKIINFLAGSTRING(CONFIG_CORRUPT       );
	CHECKIINFLAGSTRING(EVENT_BUFFER_OVERFLOW);
	return flags;
}

}

#endif // APPIINFLAGS_H
