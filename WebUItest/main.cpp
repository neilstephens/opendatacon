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
//
//  main.cpp
//  WebUItest
//
//  Created by Alan Murray on 30/08/2014.
//
//

#include <iostream>
#include "../WebUI/WebUI.h"

class StaticJsonResponder: public IJsonResponder
{
public:
	StaticJsonResponder()
	{}

	virtual Json::Value GetResponse(const ParamCollection& params) const final
	{
		Json::Value event;

		for(auto key : params)
		{
			event[key.first] = key.second;
		}

		return event;
	}
};

int
main(int argc, char *const *argv)
{
	int port = 18080;

	if (argc == 2)
	{
		port = atoi(argv[1]);
	}

	WebUI inst(port);

	const StaticJsonResponder r;

	auto sptr = std::make_shared<const StaticJsonResponder>(r);
	inst.AddJsonResponder("test", sptr);

	int status = inst.start();

	if (status)
	{
		fprintf(stderr, "Error: failed to start TLS_daemon\n");
		return 1;
	}
	else
	{
		printf("MHD daemon listening on port %d\n", port);
	}

	(void)getc(stdin);

	inst.stop();

	return 0;
}