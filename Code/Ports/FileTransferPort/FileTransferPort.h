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
 * FileTransferPort.h
 *
 *  Created on: 19/01/2023
 *      Author: Neil Stephens
 */

#ifndef FileTransferPort_H_
#define FileTransferPort_H_
#include "FileTransferPortConf.h"
#include <opendatacon/DataPort.h>

using namespace odc;

class FileTransferPort: public DataPort
{
public:
	FileTransferPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides);

	void Enable() override
	{
		pSyncStrand->dispatch([this](){Enable_();});
	}

	void Disable() override
	{
		pSyncStrand->dispatch([this](){Disable_();});
	}

	void Build() override;

	void Event(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback) override
	{
		pSyncStrand->dispatch([=](){Event_(event,SenderName,pStatusCallback);});
	}

	void ProcessElements(const Json::Value& JSONRoot) override;

private:
	//these run only on the synchronising strand
	void Periodic(asio::error_code err, std::shared_ptr<asio::steady_timer> pTimer, size_t periodms, bool only_modified);
	void Enable_();
	void Disable_();
	void Event_(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback);
	void Tx(bool only_modified);

	std::unique_ptr<asio::io_service::strand> pSyncStrand = pIOS->make_strand();
	std::string Filename = "";
	bool enabled = false;
};

#endif /* FileTransferPort_H_ */
