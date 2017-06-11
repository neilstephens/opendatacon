/*	opendatacon
 *
 *	Copyright (c) 2017:
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
 * DNP3PortManager.cpp
 *
 *  Created on: 09/05/2017
 *      Author: Alan Murray <alan@atmurray.net>
 */

#include "DNP3PortManager.h"


DNP3PortManager::DNP3PortManager(const std::shared_ptr<odc::IOManager>& pIOMgr) :
AsyncIOManager(pIOMgr),
DNP3Mgr(new asiodnp3::DNP3Manager(std::thread::hardware_concurrency()))
{

}

DNP3PortManager::~DNP3PortManager() {
	std::cout << "Destructing DNP3PortManager" << std::endl;
	DNP3Mgr->Shutdown();
}

asiodnp3::IChannel* DNP3PortManager::GetChannel(const DNP3PortConf& PortConf)
{
	std::string ChannelID;
	bool isSerial;
	if(PortConf.mAddrConf.IP.empty())
	{
		ChannelID = PortConf.mAddrConf.SerialSettings.deviceName;
		isSerial = true;
	}
	else
	{
		ChannelID = PortConf.mAddrConf.IP +":"+ std::to_string(PortConf.mAddrConf.Port);
		isSerial = false;
	}
	
	//create a new channel if needed
	if(!Channels.count(ChannelID))
	{
		if(isSerial)
		{
			Channels[ChannelID] = DNP3Mgr->AddSerial(ChannelID.c_str(), LOG_LEVEL.GetBitfield(),
												  opendnp3::ChannelRetry(
																		 openpal::TimeDuration::Milliseconds(500),
																		 openpal::TimeDuration::Milliseconds(5000)),
												  PortConf.mAddrConf.SerialSettings);
		}
		else
		{
			switch (PortConf.ClientOrServer())
			{
				case TCPClientServer::SERVER:
					Channels[ChannelID] = DNP3Mgr->AddTCPServer(ChannelID.c_str(), LOG_LEVEL.GetBitfield(),
															 opendnp3::ChannelRetry(
																					openpal::TimeDuration::Milliseconds(PortConf.pPointConf->TCPListenRetryPeriodMinms),
																					openpal::TimeDuration::Milliseconds(PortConf.pPointConf->TCPListenRetryPeriodMaxms)),
															 PortConf.mAddrConf.IP,
															 PortConf.mAddrConf.Port);
					break;
					
				case TCPClientServer::CLIENT:
					Channels[ChannelID] = DNP3Mgr->AddTCPClient(ChannelID.c_str(), LOG_LEVEL.GetBitfield(),
															 opendnp3::ChannelRetry(
																					openpal::TimeDuration::Milliseconds(PortConf.pPointConf->TCPConnectRetryPeriodMinms),
																					openpal::TimeDuration::Milliseconds(PortConf.pPointConf->TCPConnectRetryPeriodMaxms)),
															 PortConf.mAddrConf.IP,
															 "0.0.0.0",
															 PortConf.mAddrConf.Port);
					break;
					
				default:
					std::string msg = "Can't determine if TCP socket is client or server";
					throw std::runtime_error(msg);
			}
		}
	}
	
	return Channels.at(ChannelID);
}