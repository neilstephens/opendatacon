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
 * JSONServerPort.h
 *
 *  Created on: 02/11/2016
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef JSONSERVERPORT_H
#define JSONSERVERPORT_H

#include "JSONPort.h"

class JSONServerPort : public JSONPort
{
public:
	JSONServerPort(std::shared_ptr<JSONPortManager> Manager, std::string aName, std::string aConfFilename, const Json::Value aConfOverrides);

	void Enable();
	void Disable();

private:

	void ConnectCompletionHandler(asio::error_code err_code);
	std::unique_ptr<asio::strand> pEnableDisableSync;
	void PortUp();
	inline void PortUpRetry(unsigned int retry_ms)
	{
		pTCPRetryTimer.reset(new Timer_t(Manager_->get_io_service(), std::chrono::milliseconds(retry_ms)));
		pTCPRetryTimer->async_wait(pEnableDisableSync->wrap(
			[this](asio::error_code err_code)
			{
				if(err_code != asio::error::operation_aborted)
					PortUp();
			}));
	}
	void PortDown();
};

#endif // JSONSERVERPORT_H
