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

#include "../DaemonInterface.h"
#include <opendatacon/Platform.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <stdexcept>
#include <stdlib.h>
#include "../ODCArgs.h"

void daemonp(ODCArgs& Args)
{
	if(daemon(0,0))
	{
		const size_t strmax = 80;
		char buf[strmax];
		char* str = strerror_rp(errno, buf, strmax);
		std::string msg;
		if(str)
		{
			msg = std::string("Unable to daemonise: ")+str;
		}
		else
		{
			msg = std::string("Unable to daemonise: UNKNOWN ERROR");
		}
		throw std::runtime_error(msg);
	}
}
void daemon_install(ODCArgs& Args){}
void daemon_remove(){}
