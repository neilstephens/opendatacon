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
#include <random>
#include <limits>
#include <spdlog/spdlog.h>
#include "SimPort.h"
#include "SimPortConf.h"

inline unsigned int random_interval(const unsigned int& average_interval, rand_t& seed)
{
	//random interval - uniform distribution, minimum 1ms
	auto ret_val = (unsigned int)((2*average_interval-2)*ZERO_TO_ONE(seed)+1.5); //the .5 is for rounding down
	return ret_val;
}

//Implement DataPort interface
SimPort::SimPort(const std::string& Name, const std::string& File, const Json::Value& Overrides):
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
			auto pMean = pConf->AnalogStartVals.count(index) ? std::make_shared<Analog>(pConf->AnalogStartVals[index]) : std::make_shared<Analog>();
			auto std_dev = pConf->AnalogStdDevs.count(index) ? pConf->AnalogStdDevs[index] : (pMean->value ? (pConf->default_std_dev_factor*pMean->value) : 20);

			pTimer_t pTimer(new Timer_t(*pIOS));
			Timers.push_back(pTimer);

			//use a heap pointer as a random seed
			auto seed = (rand_t)((intptr_t)(pTimer.get()));

			pTimer->expires_from_now(std::chrono::milliseconds(random_interval(interval, seed)));
			pTimer->async_wait([=](asio::error_code err_code)
				{
					//FIXME: check err_code?
					if(enabled)
						SpawnEvent(pMean, std_dev, interval, index, pTimer, seed);
				});
		}
	}
	for(auto index : pConf->BinaryIndicies)
	{
		if(pConf->BinaryUpdateIntervalms.count(index))
		{
			auto interval = pConf->BinaryUpdateIntervalms[index];
			auto pVal = pConf->BinaryStartVals.count(index) ? std::make_shared<Binary>(pConf->BinaryStartVals[index]) : std::make_shared<Binary>();

			pTimer_t pTimer(new Timer_t(*pIOS));
			Timers.push_back(pTimer);

			//use a heap pointer as a random seed
			auto seed = (rand_t)((intptr_t)(pTimer.get()));

			pTimer->expires_from_now(std::chrono::milliseconds(random_interval(interval, seed)));
			pTimer->async_wait([=](asio::error_code err_code)
				{
					//FIXME: check err_code?
					if(enabled)
						SpawnEvent(pVal, interval, index, pTimer, seed);
				});
		}
	}
}

void SimPort::PortDown()
{
	for(auto pTimer : Timers)
		pTimer->cancel();
	Timers.clear();
}

void SimPort::SpawnEvent(std::shared_ptr<Analog> pMean, double std_dev, unsigned int interval, size_t index, pTimer_t pTimer, rand_t seed)
{
	//Restart the timer
	pTimer->expires_from_now(std::chrono::milliseconds(random_interval(interval, seed)));

	//Send an event out
	//change value around mean
	std::normal_distribution<double> distribution(pMean->value, std_dev);
	double val = distribution(RandNumGenerator);
	PublishEvent(Analog(val, pMean->quality, Timestamp(msSinceEpoch())), index);
	auto event = std::make_shared<EventInfo>(EventType::Analog, Name);
	event->SetPayload<EventType::Analog>(std::move(val));
	PublishEvent(event);

	//wait til next time
	pTimer->async_wait([=](asio::error_code err_code)
		{
			//FIXME: check err_code?
			if(enabled)
				SpawnEvent(pMean,std_dev,interval,index,pTimer,seed);
			//else - break timer cycle
		});
}

void SimPort::SpawnEvent(std::shared_ptr<Binary> pVal, unsigned int interval, size_t index, pTimer_t pTimer, rand_t seed)
{
	//Restart the timer
	pTimer->expires_from_now(std::chrono::milliseconds(random_interval(interval, seed)));

	//Send an event out
	//toggle value
	pVal->value = !pVal->value;
	pVal->time = Timestamp(std::chrono::duration_cast<std::chrono::milliseconds>
			(std::chrono::system_clock::now().time_since_epoch()).count());
	//pass a copy, because we don't know when the ref will go out of scope
	PublishEvent(Binary(*pVal), index);

	//wait til next time
	pTimer->async_wait([=](asio::error_code err_code)
		{
			//FIXME: check err_code?
			if(enabled)
				SpawnEvent(pVal,interval,index,pTimer,seed);
			//else - break timer cycle
		});
}

void SimPort::BuildOrRebuild()
{
	pEnableDisableSync.reset(new asio::strand(*pIOS));
}

void SimPort::ProcessElements(const Json::Value& JSONRoot)
{
	auto pConf = static_cast<SimPortConf*>(this->pConf.get());

	if(JSONRoot.isMember("Analogs"))
	{
		const auto Analogs = JSONRoot["Analogs"];
		for(Json::ArrayIndex n = 0; n < Analogs.size(); ++n)
		{
			size_t start, stop;
			if(Analogs[n].isMember("Index"))
				start = stop = Analogs[n]["Index"].asUInt();
			else if(Analogs[n]["Range"].isMember("Start") && Analogs[n]["Range"].isMember("Stop"))
			{
				start = Analogs[n]["Range"]["Start"].asUInt();
				stop = Analogs[n]["Range"]["Stop"].asUInt();
			}
			else
			{
				if(auto log = spdlog::get("SimPort"))
					log->error("A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '{}'", Analogs[n].toStyledString());
				continue;
			}
			for(auto index = start; index <= stop; index++)
			{
				bool exists = false;
				for(auto existing_index : pConf->AnalogIndicies)
					if(existing_index == index)
						exists = true;

				if(!exists)
					pConf->AnalogIndicies.push_back(index);

				if(Analogs[n].isMember("StdDev"))
					pConf->AnalogStdDevs[index] = Analogs[n]["StdDev"].asDouble();
				if(Analogs[n].isMember("UpdateIntervalms"))
					pConf->AnalogUpdateIntervalms[index] = Analogs[n]["UpdateIntervalms"].asUInt();

				if(Analogs[n].isMember("StartVal"))
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
					else if(start_val == "NAN" || start_val == "nan" || start_val == "NaN")
					{
						pConf->AnalogStartVals[index] = Analog(std::numeric_limits<double>::quiet_NaN(),static_cast<uint8_t>(AnalogQuality::ONLINE));
					}
					else if(start_val == "INF" || start_val == "inf")
					{
						pConf->AnalogStartVals[index] = Analog(std::numeric_limits<double>::infinity(),static_cast<uint8_t>(AnalogQuality::ONLINE));
					}
					else if(start_val == "-INF" || start_val == "-inf")
					{
						pConf->AnalogStartVals[index] = Analog(-std::numeric_limits<double>::infinity(),static_cast<uint8_t>(AnalogQuality::ONLINE));
					}
					else if(start_val == "X")
						pConf->AnalogStartVals[index] = Analog(0,static_cast<uint8_t>(AnalogQuality::COMM_LOST));
					else
						pConf->AnalogStartVals[index] = Analog(std::stod(start_val),static_cast<uint8_t>(AnalogQuality::ONLINE));
				}
				else if(pConf->AnalogStartVals.count(index))
					pConf->AnalogStartVals.erase(index);
			}
		}
		std::sort(pConf->AnalogIndicies.begin(),pConf->AnalogIndicies.end());
	}

	if(JSONRoot.isMember("Binaries"))
	{
		const auto Binaries = JSONRoot["Binaries"];
		for(Json::ArrayIndex n = 0; n < Binaries.size(); ++n)
		{
			size_t start, stop;
			if(Binaries[n].isMember("Index"))
				start = stop = Binaries[n]["Index"].asUInt();
			else if(Binaries[n]["Range"].isMember("Start") && Binaries[n]["Range"].isMember("Stop"))
			{
				start = Binaries[n]["Range"]["Start"].asUInt();
				stop = Binaries[n]["Range"]["Stop"].asUInt();
			}
			else
			{
				if(auto log = spdlog::get("SimPort"))
					log->error("A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '{}'", Binaries[n].toStyledString());
				continue;
			}
			for(auto index = start; index <= stop; index++)
			{

				bool exists = false;
				for(auto existing_index : pConf->BinaryIndicies)
					if(existing_index == index)
						exists = true;

				if(!exists)
					pConf->BinaryIndicies.push_back(index);

				if(Binaries[n].isMember("UpdateIntervalms"))
					pConf->BinaryUpdateIntervalms[index] = Binaries[n]["UpdateIntervalms"].asUInt();

				if(Binaries[n].isMember("StartVal"))
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
						pConf->BinaryStartVals[index] = Binary(false,static_cast<uint8_t>(BinaryQuality::COMM_LOST));
					else
						pConf->BinaryStartVals[index] = Binary(Binaries[n]["StartVal"].asBool(),static_cast<uint8_t>(BinaryQuality::ONLINE));
				}
				else if(pConf->BinaryStartVals.count(index))
					pConf->BinaryStartVals.erase(index);
			}
		}
		std::sort(pConf->BinaryIndicies.begin(),pConf->BinaryIndicies.end());
	}

	if(JSONRoot.isMember("BinaryControls"))
	{
		const auto BinaryControls= JSONRoot["BinaryControls"];
		for(Json::ArrayIndex n = 0; n < BinaryControls.size(); ++n)
		{
			size_t start, stop;
			if(BinaryControls[n].isMember("Index"))
				start = stop = BinaryControls[n]["Index"].asUInt();
			else if(BinaryControls[n]["Range"].isMember("Start") && BinaryControls[n]["Range"].isMember("Stop"))
			{
				start = BinaryControls[n]["Range"]["Start"].asUInt();
				stop = BinaryControls[n]["Range"]["Stop"].asUInt();
			}
			else
			{
				if(auto log = spdlog::get("SimPort"))
					log->error("A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '{}'", BinaryControls[n].toStyledString());
				continue;
			}
			for(auto index = start; index <= stop; index++)
			{
				bool exists = false;
				for(auto existing_index : pConf->ControlIndicies)
					if(existing_index == index)
						exists = true;

				if(!exists)
					pConf->ControlIndicies.push_back(index);

				if(BinaryControls[n].isMember("Intervalms"))
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

				if(BinaryControls[n].isMember("FeedbackBinaries"))
				{
					const auto FeedbackBinaries= BinaryControls[n]["FeedbackBinaries"];
					for(Json::ArrayIndex fbn = 0; fbn < FeedbackBinaries.size(); ++fbn)
					{
						if(!FeedbackBinaries[fbn].isMember("Index"))
						{
							if(auto log = spdlog::get("SimPort"))
								log->error("An 'Index' is required for Binary feedback : '{}'",FeedbackBinaries[fbn].toStyledString());
							continue;
						}

						BinaryFeedback fb(FeedbackBinaries[fbn]["Index"].asUInt());
						if(FeedbackBinaries[fbn].isMember("OnValue"))
						{
							if(FeedbackBinaries[fbn]["OnValue"].asString() == "X")
								fb.on_value = Binary(false,static_cast<uint8_t>(BinaryQuality::COMM_LOST));
							else
								fb.on_value = Binary(FeedbackBinaries[fbn]["OnValue"].asBool(),static_cast<uint8_t>(BinaryQuality::ONLINE));
						}
						if(FeedbackBinaries[fbn].isMember("OffValue"))
						{
							if(FeedbackBinaries[fbn]["OffValue"].asString() == "X")
								fb.off_value = Binary(false,static_cast<uint8_t>(BinaryQuality::COMM_LOST));
							else
								fb.off_value = Binary(FeedbackBinaries[fbn]["OffValue"].asBool(),static_cast<uint8_t>(BinaryQuality::ONLINE));

						}
						if(FeedbackBinaries[fbn].isMember("FeedbackMode"))
						{
							auto mode = FeedbackBinaries[fbn]["FeedbackMode"].asString();
							if(mode == "PULSE")
								fb.mode = FeedbackMode::PULSE;
							else if(mode == "LATCH")
								fb.mode = FeedbackMode::LATCH;
							else
							{
								if(auto log = spdlog::get("SimPort"))
									log->warn("Unrecognised feedback mode: '{}'",FeedbackBinaries[fbn].toStyledString());
							}
						}
						pConf->ControlFeedback[index].push_back(std::move(fb));
					}
				}
			}
		}
		std::sort(pConf->ControlIndicies.begin(),pConf->ControlIndicies.end());
	}
}
void SimPort::ConnectionEvent(ConnectState state, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::SUCCESS);
}

//Implement Event handlers from IOHandler - All not supported because SimPort is just a source.

// measurement events
void SimPort::Event(const Binary& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const DoubleBitBinary& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const Analog& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const Counter& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const FrozenCounter& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const BinaryOutputStatus& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const AnalogOutputStatus& meas, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}

// change of quality Events
void SimPort::Event(const BinaryQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const DoubleBitBinaryQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const AnalogQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const CounterQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const FrozenCounterQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const BinaryOutputStatusQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const AnalogOutputStatusQuality qual, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
// control events
void SimPort::Event(const ControlRelayOutputBlock& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	auto pConf = static_cast<SimPortConf*>(this->pConf.get());
	for(auto i : pConf->ControlIndicies)
	{
		if(i == index)
		{
			if(pConf->ControlFeedback.count(i))
			{
				for(auto& fb : pConf->ControlFeedback[i])
				{
					if(fb.mode == PULSE)
					{
						switch(arCommand.functionCode)
						{
							case ControlCode::PULSE_ON:
							case ControlCode::LATCH_ON:
							case ControlCode::LATCH_OFF:
							case ControlCode::CLOSE_PULSE_ON:
							case ControlCode::TRIP_PULSE_ON:
							{
								PublishEvent(fb.on_value,fb.binary_index);
								pTimer_t pTimer(new Timer_t(*pIOS));
								pTimer->expires_from_now(std::chrono::milliseconds(arCommand.onTimeMS));
								pTimer->async_wait([pTimer,fb,this](asio::error_code err_code)
									{
										//FIXME: check err_code?
										PublishEvent(fb.off_value,fb.binary_index);
									});
								//TODO: (maybe) implement multiple pulses - command has count and offTimeMS
								break;
							}
							default:
								(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
								return;
						}
					}
					else
					{
						switch(arCommand.functionCode)
						{
							case ControlCode::LATCH_ON:
							case ControlCode::CLOSE_PULSE_ON:
								PublishEvent(fb.on_value,fb.binary_index);
								break;
							case ControlCode::LATCH_OFF:
							case ControlCode::TRIP_PULSE_ON:
								PublishEvent(fb.off_value,fb.binary_index);
								break;
							default:
								(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
								return;
						}
					}
				}
				(*pStatusCallback)(CommandStatus::SUCCESS);
				return;
			}
			(*pStatusCallback)(CommandStatus::UNDEFINED);
			return;
		}
	}
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const AnalogOutputInt16& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const AnalogOutputInt32& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const AnalogOutputFloat32& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(const AnalogOutputDouble64& arCommand, uint16_t index, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}
void SimPort::Event(std::shared_ptr<EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	(*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
}

