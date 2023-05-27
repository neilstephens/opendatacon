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
#ifndef ODCARGS_H
#define ODCARGS_H

#include <tclap/CmdLine.h>
#include <opendatacon/version.h>
struct ODCArgs
{
	ODCArgs(int argc, char* argv[]):
		cmd("High performance asynchronous data concentrator", ' ', odc::version_string()),
		ConfigFileArg("c", "config", "Configuration file, specified as an absolute path or relative to the working directory.", false, "opendatacon.conf", "string"),
		PathArg("p", "path", "Working directory path, all configuration files and log files are relative to this path.", false, "", "string"),
		DaemonInstallArg("i", "daemon_install", "Switch to install opendatacon as a background service (not required / ignored for POSIX platforms)"),
		DaemonArg("d", "daemon", "Switch to run opendatacon in the background"),
		PIDFileArg("f", "pidfile", "Optional file path to write a pid file in daemon mode. Eg. /var/run/opendatacon.pid", false, "", "string"),
		DaemonRemoveArg("r", "daemon_remove", "Switch to uninstall opendatacon as a background service (not required / ignored for POSIX platforms)"),
		ConcurrencyArg("x", "concurrency_hint", "A hint for the number of threads in thread pools etc.",false,0,"positive integer")
	{
		cmd.add(ConfigFileArg);
		cmd.add(PathArg);
		cmd.add(DaemonInstallArg);
		cmd.add(DaemonArg);
		cmd.add(PIDFileArg);
		cmd.add(DaemonRemoveArg);
		cmd.add(ConcurrencyArg);
		cmd.parse(argc, argv);
	}
	TCLAP::CmdLine cmd;
	TCLAP::ValueArg<std::string> ConfigFileArg;
	TCLAP::ValueArg<std::string> PathArg;
	TCLAP::SwitchArg DaemonInstallArg;
	TCLAP::SwitchArg DaemonArg;
	TCLAP::ValueArg<std::string> PIDFileArg;
	TCLAP::SwitchArg DaemonRemoveArg;
	TCLAP::ValueArg<int> ConcurrencyArg;

	std::string toString()
	{
		std::string fullstring;
		for (auto arg : cmd.getArgList())
		{
			if (arg->isSet())
			{
				fullstring += " -" + arg->getFlag();
				auto asStrValueArg = dynamic_cast<TCLAP::ValueArg<std::string>*>(arg);
				if (asStrValueArg != nullptr)
				{
					fullstring += " " + asStrValueArg->getValue();
				}
				auto asIntValueArg = dynamic_cast<TCLAP::ValueArg<int>*>(arg);
				if (asIntValueArg != nullptr)
				{
					fullstring += " " + std::to_string(asIntValueArg->getValue());
				}
			}
		}

		return fullstring;
	}
};

#endif
