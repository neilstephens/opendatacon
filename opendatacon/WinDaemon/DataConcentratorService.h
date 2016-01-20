/****************************** Module Header ******************************\
* Module Name:  SampleService.h
* Project:      CppWindowsService
* Copyright (c) Microsoft Corporation.
*
* Provides a sample service class that derives from the service base class -
* CServiceBase. The sample service logs the service start and stop
* information to the Application event log, and shows how to run the main
* function of the service in a thread pool worker thread.
*
* This source is subject to the Microsoft Public License.
* See http://www.microsoft.com/en-us/openness/resources/licenses.aspx#MPL.
* All other rights reserved.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
* EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/

#pragma once

#include "../DataConcentrator.h"
#include "ServiceBase.h"
#include "../ODCArgs.h"

class DataConcentratorService: public CServiceBase
{
public:

	DataConcentratorService(ODCArgs& Args,
	                        PWSTR pszServiceName,
	                        BOOL fCanStop = TRUE,
	                        BOOL fCanShutdown = TRUE,
	                        BOOL fCanPauseContinue = FALSE);
	virtual ~DataConcentratorService(void);

protected:

	virtual void OnStart(DWORD dwArgc, PWSTR *pszArgv);
	virtual void OnStop();

	void ServiceWorkerThread(void);

private:
	ODCArgs& Args;
	std::unique_ptr<DataConcentrator> TheDataConcentrator;
	BOOL m_fStopping;
	HANDLE m_hStoppedEvent;
};