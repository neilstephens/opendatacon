/****************************** Module Header ******************************\
* Module Name:  CppWindowsService.cpp
* Project:      CppWindowsService
* Copyright (c) Microsoft Corporation.
* 
* The file defines the entry point of the application. According to the 
* arguments in the command line, the function installs or uninstalls or 
* starts the service by calling into different routines.
* 
* This source is subject to the Microsoft Public License.
* See http://www.microsoft.com/en-us/openness/resources/licenses.aspx#MPL.
* All other rights reserved.
* 
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, 
* EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
* WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/

#pragma region Includes
#include <stdio.h>
#include <windows.h>
#include "ServiceInstaller.h"
#include "ServiceBase.h"
#include "DataConcentratorService.h"
#include "../DaemonInterface.h"
#pragma endregion

// 
// Settings of the service
// 

// Internal name of the service
#define SERVICE_NAME             L"opendatacon"

// Displayed name of the service
#define SERVICE_DISPLAY_NAME     L"Opendatacon Service"

// Service start options.
#define SERVICE_START_TYPE       SERVICE_DEMAND_START

// List of service dependencies - "dep1\0dep2\0\0"
#define SERVICE_DEPENDENCIES     L""

// The name of the account under which the service should run
#define SERVICE_ACCOUNT          L"NT AUTHORITY\\LocalService"

// The password to the service account name
#define SERVICE_PASSWORD         NULL

void daemonp(ODCArgs& Args)
{
	DataConcentratorService service(Args, SERVICE_NAME);
	if (!CServiceBase::Run(service))
	{
		wprintf(L"Service failed to run w/err 0x%08lx\n", GetLastError());
	}
	exit(0);
}

void daemon_install(ODCArgs& Args)
{
	Args.DaemonInstallArg.reset();
	std::wstringstream wss;
	wss << " -d" << Args.toString().c_str();

	InstallService(
	SERVICE_NAME,               // Name of service
	SERVICE_DISPLAY_NAME,       // Name to display
	SERVICE_START_TYPE,         // Service start type
	SERVICE_DEPENDENCIES,       // Dependencies
	SERVICE_ACCOUNT,            // Service running account
	SERVICE_PASSWORD,            // Password of the account
	(PWSTR)wss.str().c_str());
}

void daemon_remove()
{
	UninstallService(SERVICE_NAME);
}