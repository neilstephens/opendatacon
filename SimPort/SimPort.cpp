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
 * SimPort.h
 *
 *  Created on: 29/07/2015
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include "SimPort.h"
#include "SimPortConf.h"
#include <random>

inline unsigned int random_interval(const unsigned int& average_interval, rand_t& seed)
{
	//random interval - uniform distribution, minimum 1ms
	auto ret_val = (unsigned int)((2*average_interval-2)*ZERO_TO_ONE(seed)+1.5); //the .5 is for rounding down
	return ret_val;
}

//Implement DataPort interface
SimPort::SimPort(std::string Name, std::string File, const Json::Value Overrides):
	DataPort(Name, File, Overrides),
	enabled(false)
{
	pConf.reset(new SimPortConf());
	ProcessFile();
}
void SimPort::Enable()
{
	pEnableDisableSync->post([&]()
	                         {
	                               if(!enabled)
	                               {
	                                     enabled = true;
	                                     PortUp();
						 }
					 });
}
void SimPort::Disable()
{
	pEnableDisableSync->post([&]()
	                         {
	                               if(enabled)
	                               {
	                                     enabled = false;
	                                     PortDown();
						 }
					 });
}

void SimPort::PortUp()
{
	auto pConf = static_cast<SimPortConf*>(this->pConf.get());
	for(auto index : pConf->AnalogIndicies)
	{
		if(pConf->AnalogUpdateIntervalms.count(index))
		{
			auto interval = pConf->AnalogUpdateIntervalms[index];
			auto pMean = pConf->AnalogStartVals.count(index) ? std::make_shared<opendnp3::Analog>(pConf->AnalogStartVals[index]) : std::make_shared<opendnp3::Analog>();
			auto std_dev = pConf->AnalogStdDevs.count(index) ? pConf->AnalogStdDevs[index] : (pMean->value ? (pConf->default_std_dev_factor*pMean->value) : 20);

			pTimer_t pTimer(new Timer_t(*pIOS));
			Timers.push_back(pTimer);

			//use a heap pointer as a random seed
			auto seed = (rand_t)((intptr_t)(pTimer.get()));

			pTimer->expires_from_now(std::chrono::milliseconds(random_interval(interval, seed)));
			pTimer->async_wait([=](asio::error_code err_code)
			                   {
			                         if(err_code != asio::error::operation_aborted)
								 SpawnEvent(pMean, std_dev, interval, index, pTimer, seed);
						 });
		}
	}
}

void SimPort::PortDown()
{
	while(Timers.size() > 0)
	{
		pTimer_t pTimer = Timers.back();
		Timers.pop_back();
		pTimer->cancel();
		pTimer->wait(); //waiting ensures all timers will be destroyed here
	}
}

void SimPort::SpawnEvent(std::shared_ptr<opendnp3::Analog> pMean, double std_dev, unsigned int interval, size_t index, pTimer_t pTimer, rand_t seed)
{
	//This breaks us out of the timer cycle if the timer was cancelled (port disabled), but we were already queued
	if(!enabled)
		return;

	//Restart the timer
	pTimer->expires_from_now(std::chrono::milliseconds(random_interval(interval, seed)));

	//Send an event out
	for (auto IOHandler_pair : Subscribers)
	{
		//change value around mean
		std::normal_distribution<double> distribution(pMean->value, std_dev);
		IOHandler_pair.second->Event(opendnp3::Analog(distribution(RandNumGenerator),pMean->quality), index, this->Name);
	}

	//wait til next time
	pTimer->async_wait([=](asio::error_code err_code)
	                   {
	                         if(err_code != asio::error::operation_aborted)
						 SpawnEvent(pMean,std_dev,interval,index,pTimer,seed);
					//else cancelled - break timer cycle
				 });
}

void SimPort::BuildOrRebuild(asiodnp3::DNP3Manager& DNP3Mgr, openpal::LogFilters& LOG_LEVEL)
{
	pEnableDisableSync.reset(new asio::strand(*pIOS));
}

void SimPort::ProcessElements(const Json::Value& JSONRoot)
{
	auto pConf = static_cast<SimPortConf*>(this->pConf.get());

	if(!JSONRoot["Analogs"].isNull())
	{
		const auto Analogs = JSONRoot["Analogs"];
		for(Json::ArrayIndex n = 0; n < Analogs.size(); ++n)
		{
			size_t start, stop;
			if(!Analogs[n]["Index"].isNull())
				start = stop = Analogs[n]["Index"].asUInt();
			else if(!Analogs[n]["Range"]["Start"].isNull() && !Analogs[n]["Range"]["Stop"].isNull())
			{
				start = Analogs[n]["Range"]["Start"].asUInt();
				stop = Analogs[n]["Range"]["Stop"].asUInt();
			}
			else
			{
				std::cout<<"A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '"<<Analogs[n].toStyledString()<<"'"<<std::endl;
				start = 1;
				stop = 0;
			}
			for(auto index = start; index <= stop; index++)
			{
				bool exists = false;
				for(auto existing_index : pConf->AnalogIndicies)
					if(existing_index == index)
						exists = true;

				if(!exists)
					pConf->AnalogIndicies.push_back(index);

				if(!Analogs[n]["StdDev"].isNull())
					pConf->AnalogStdDevs[index] = Analogs[n]["StdDev"].asDouble();
				if(!Analogs[n]["UpdateIntervalms"].isNull())
					pConf->AnalogUpdateIntervalms[index] = Analogs[n]["UpdateIntervalms"].asUInt();

				if(!Analogs[n]["StartVal"].isNull())
				{
					std::string start_val = Analogs[n]["StartVal"].asString();
					if(start_val == "D") //delete this index
					{
						if(pConf->AnalogStartVals.count(index))
							pConf->AnalogStartVals.erase(index);
						if(pConf->AnalogStdDevs.count(index))
							pConf->AnalogStdDevs.erase(index);
						if(pConf->AnalogUpdateIntervalms.count(index))
							pConf->AnalogUpdateIntervalms.erase(index);
						for(auto it = pConf->AnalogIndicies.begin(); it != pConf->AnalogIndicies.end(); it++)
							if(*it == index)
							{
								pConf->AnalogIndicies.erase(it);
								break;
							}
					}
					else if(start_val == "X")
						pConf->AnalogStartVals[index] = opendnp3::Analog(0,static_cast<uint8_t>(opendnp3::AnalogQuality::COMM_LOST));
					else
						pConf->AnalogStartVals[index] = opendnp3::Analog(std::stod(start_val),static_cast<uint8_t>(opendnp3::AnalogQuality::ONLINE));
				}
				else if(pConf->AnalogStartVals.count(index))
					pConf->AnalogStartVals.erase(index);
			}
		}
		std::sort(pConf->AnalogIndicies.begin(),pConf->AnalogIndicies.end());
	}

	if(!JSONRoot["Binaries"].isNull())
	{
		const auto Binaries = JSONRoot["Binaries"];
		for(Json::ArrayIndex n = 0; n < Binaries.size(); ++n)
		{
			size_t start, stop;
			if(!Binaries[n]["Index"].isNull())
				start = stop = Binaries[n]["Index"].asUInt();
			else if(!Binaries[n]["Range"]["Start"].isNull() && !Binaries[n]["Range"]["Stop"].isNull())
			{
				start = Binaries[n]["Range"]["Start"].asUInt();
				stop = Binaries[n]["Range"]["Stop"].asUInt();
			}
			else
			{
				std::cout<<"A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '"<<Binaries[n].toStyledString()<<"'"<<std::endl;
				start = 1;
				stop = 0;
			}
			for(auto index = start; index <= stop; index++)
			{

				bool exists = false;
				for(auto existing_index : pConf->BinaryIndicies)
					if(existing_index == index)
						exists = true;

				if(!exists)
					pConf->BinaryIndicies.push_back(index);

				if(!Binaries[n]["UpdateIntervalms"].isNull())
					pConf->BinaryUpdateIntervalms[index] = Binaries[n]["UpdateIntervalms"].asUInt();

				if(!Binaries[n]["StartVal"].isNull())
				{
					std::string start_val = Binaries[n]["StartVal"].asString();
					if(start_val == "D") //delete this index
					{
						if(pConf->BinaryStartVals.count(index))
							pConf->BinaryStartVals.erase(index);
						if(pConf->BinaryUpdateIntervalms.count(index))
							pConf->BinaryUpdateIntervalms.erase(index);
						for(auto it = pConf->BinaryIndicies.begin(); it != pConf->BinaryIndicies.end(); it++)
							if(*it == index)
							{
								pConf->BinaryIndicies.erase(it);
								break;
							}
					}
					else if(start_val == "X")
						pConf->BinaryStartVals[index] = opendnp3::Binary(false,static_cast<uint8_t>(opendnp3::BinaryQuality::COMM_LOST));
					else
						pConf->BinaryStartVals[index] = opendnp3::Binary(Binaries[n]["StartVal"].asBool(),static_cast<uint8_t>(opendnp3::BinaryQuality::ONLINE));
				}
				else if(pConf->BinaryStartVals.count(index))
					pConf->BinaryStartVals.erase(index);
			}
		}
		std::sort(pConf->BinaryIndicies.begin(),pConf->BinaryIndicies.end());
	}

	if(!JSONRoot["BinaryControls"].isNull())
	{
		const auto BinaryControls= JSONRoot["BinaryControls"];
		for(Json::ArrayIndex n = 0; n < BinaryControls.size(); ++n)
		{
			size_t start, stop;
			if(!BinaryControls[n]["Index"].isNull())
				start = stop = BinaryControls[n]["Index"].asUInt();
			else if(!BinaryControls[n]["Range"]["Start"].isNull() && !BinaryControls[n]["Range"]["Stop"].isNull())
			{
				start = BinaryControls[n]["Range"]["Start"].asUInt();
				stop = BinaryControls[n]["Range"]["Stop"].asUInt();
			}
			else
			{
				std::cout<<"A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '"<<BinaryControls[n].toStyledString()<<"'"<<std::endl;
				start = 1;
				stop = 0;
			}
			for(auto index = start; index <= stop; index++)
			{
				bool exists = false;
				for(auto existing_index : pConf->ControlIndicies)
					if(existing_index == index)
						exists = true;

				if(!exists)
					pConf->ControlIndicies.push_back(index);

				if(!BinaryControls[n]["Intervalms"].isNull())
					pConf->ControlIntervalms[index] = BinaryControls[n]["Intervalms"].asUInt();

				auto start_val = BinaryControls[n]["StartVal"].asString();
				if(start_val == "D")
				{
					if(pConf->ControlIntervalms.count(index))
						pConf->ControlIntervalms.erase(index);
					for(auto it = pConf->ControlIndicies.begin(); it != pConf->ControlIndicies.end(); it++)
						if(*it == index)
						{
							pConf->ControlIndicies.erase(it);
							break;
						}
				}
			}
		}
		std::sort(pConf->ControlIndicies.begin(),pConf->ControlIndicies.end());
	}
}
std::future<opendnp3::CommandStatus> SimPort::ConnectionEvent(ConnectState state, const std::string& SenderName)
{
	return CommandFutureSuccess();
}

//Implement Event handlers from IOHandler - All not supported because SimPort is just a source.

// measurement events
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::Binary& meas, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::DoubleBitBinary& meas, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::Analog& meas, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::Counter& meas, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::FrozenCounter& meas, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}

// change of quality Events
std::future<opendnp3::CommandStatus> SimPort::Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const CounterQuality qual, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}

// control events
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}
std::future<opendnp3::CommandStatus> SimPort::Event(const opendnp3::AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName)
{
	return CommandFutureNotSupported();
}

