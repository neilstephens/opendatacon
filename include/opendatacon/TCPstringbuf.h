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
 * TCPstringbuf.h
 *
 *  Created on: 2018-06-19
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */
#include "TCPSocketManager.h"
#include <sstream>

namespace odc
{

//implement a std::stringbuf with a TCP connection
//for use in a std::ostream
class TCPstringbuf: public std::stringbuf
{
public:
	void Init(std::shared_ptr<odc::asio_service> apIOS,
		bool aisServer,
		const std::string& aEndPoint,
		const std::string& aPort
		)
	{
		pIOS = apIOS;
		pSockMan = std::make_unique<TCPSocketManager<std::string>>(apIOS,aisServer,aEndPoint,aPort,
			[](buf_t& readbuf){},[](bool state){},1000,true);
		pSockMan->Open();
	}
	void DeInit()
	{
		if(pSockMan)
			pSockMan->Close();
		pSockMan.reset();
	}
	int sync() override
	{
		if(pSockMan)
		{
			pSockMan->Write(str()); //write
			str("");                //clear
			return 0;               //success
		}
		return -1; //fail
	}
private:
	std::shared_ptr<odc::asio_service> pIOS;
	std::unique_ptr<TCPSocketManager<std::string>> pSockMan;
};

} //namespace odc
