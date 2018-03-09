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
 * MD3PointConf.cpp
 *
 *  Created on: 16/10/2014
 *      Author: Alan Murray
 */

#include <regex>
#include <algorithm>
#include "MD3PointConf.h"
#include <opendatacon/util.h>
#include <opendatacon/IOTypes.h>

using namespace odc;

MD3PointConf::MD3PointConf(std::string FileName):
	ConfigParser(FileName)
{
	ProcessFile();
}


void MD3PointConf::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	// Root level Configuration values
	if (JSONRoot.isMember("LinkNumRetry"))
		LinkNumRetry = JSONRoot["LinkNumRetry"].asUInt();
	if (JSONRoot.isMember("LinkTimeoutms"))
		LinkTimeoutms = JSONRoot["LinkTimeoutms"].asUInt();

		if (JSONRoot.isMember("Analogs"))
		{
			const auto Analogs = JSONRoot["Analogs"];
			for (Json::ArrayIndex n = 0; n < Analogs.size(); ++n)
			{
				size_t start, stop;
				if (Analogs[n].isMember("Index"))
					start = stop = Analogs[n]["Index"].asUInt();
				else if (Analogs[n]["Range"].isMember("Start") && Analogs[n]["Range"].isMember("Stop"))
				{
					start = Analogs[n]["Range"]["Start"].asUInt();
					stop = Analogs[n]["Range"]["Stop"].asUInt();
				}
				else
				{
					std::cout << "A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '" << Analogs[n].toStyledString() << "'" << std::endl;
					start = 1;
					stop = 0;
				}
				for (auto index = start; index <= stop; index++)
				{
					bool exists = false;
					for (auto existing_index : pConf->AnalogIndicies)
						if (existing_index == index)
							exists = true;

					if (!exists)
						pConf->AnalogIndicies.push_back(index);

					if (Analogs[n].isMember("StdDev"))
						pConf->AnalogStdDevs[index] = Analogs[n]["StdDev"].asDouble();
					if (Analogs[n].isMember("UpdateIntervalms"))
						pConf->AnalogUpdateIntervalms[index] = Analogs[n]["UpdateIntervalms"].asUInt();

					if (Analogs[n].isMember("StartVal"))
					{
						std::string start_val = Analogs[n]["StartVal"].asString();
						if (start_val == "D") //delete this index
						{
							if (pConf->AnalogStartVals.count(index))
								pConf->AnalogStartVals.erase(index);
							if (pConf->AnalogStdDevs.count(index))
								pConf->AnalogStdDevs.erase(index);
							if (pConf->AnalogUpdateIntervalms.count(index))
								pConf->AnalogUpdateIntervalms.erase(index);
							for (auto it = pConf->AnalogIndicies.begin(); it != pConf->AnalogIndicies.end(); it++)
								if (*it == index)
								{
									pConf->AnalogIndicies.erase(it);
									break;
								}
						}
						else if (start_val == "NAN" || start_val == "nan" || start_val == "NaN")
						{
							pConf->AnalogStartVals[index] = Analog(std::numeric_limits<double>::quiet_NaN(), static_cast<uint8_t>(AnalogQuality::ONLINE));
						}
						else if (start_val == "INF" || start_val == "inf")
						{
							pConf->AnalogStartVals[index] = Analog(std::numeric_limits<double>::infinity(), static_cast<uint8_t>(AnalogQuality::ONLINE));
						}
						else if (start_val == "-INF" || start_val == "-inf")
						{
							pConf->AnalogStartVals[index] = Analog(-std::numeric_limits<double>::infinity(), static_cast<uint8_t>(AnalogQuality::ONLINE));
						}
						else if (start_val == "X")
							pConf->AnalogStartVals[index] = Analog(0, static_cast<uint8_t>(AnalogQuality::COMM_LOST));
						else
							pConf->AnalogStartVals[index] = Analog(std::stod(start_val), static_cast<uint8_t>(AnalogQuality::ONLINE));
					}
					else if (pConf->AnalogStartVals.count(index))
						pConf->AnalogStartVals.erase(index);
				}
			}
			std::sort(pConf->AnalogIndicies.begin(), pConf->AnalogIndicies.end());
		}

		if (JSONRoot.isMember("Binaries"))
		{
			const auto Binaries = JSONRoot["Binaries"];
			for (Json::ArrayIndex n = 0; n < Binaries.size(); ++n)
			{
				size_t start, stop;
				if (Binaries[n].isMember("Index"))
					start = stop = Binaries[n]["Index"].asUInt();
				else if (Binaries[n]["Range"].isMember("Start") && Binaries[n]["Range"].isMember("Stop"))
				{
					start = Binaries[n]["Range"]["Start"].asUInt();
					stop = Binaries[n]["Range"]["Stop"].asUInt();
				}
				else
				{
					std::cout << "A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '" << Binaries[n].toStyledString() << "'" << std::endl;
					start = 1;
					stop = 0;
				}
				for (auto index = start; index <= stop; index++)
				{

					bool exists = false;
					for (auto existing_index : pConf->BinaryIndicies)
						if (existing_index == index)
							exists = true;

					if (!exists)
						pConf->BinaryIndicies.push_back(index);

					if (Binaries[n].isMember("UpdateIntervalms"))
						pConf->BinaryUpdateIntervalms[index] = Binaries[n]["UpdateIntervalms"].asUInt();

					if (Binaries[n].isMember("StartVal"))
					{
						std::string start_val = Binaries[n]["StartVal"].asString();
						if (start_val == "D") //delete this index
						{
							if (pConf->BinaryStartVals.count(index))
								pConf->BinaryStartVals.erase(index);
							if (pConf->BinaryUpdateIntervalms.count(index))
								pConf->BinaryUpdateIntervalms.erase(index);
							for (auto it = pConf->BinaryIndicies.begin(); it != pConf->BinaryIndicies.end(); it++)
								if (*it == index)
								{
									pConf->BinaryIndicies.erase(it);
									break;
								}
						}
						else if (start_val == "X")
							pConf->BinaryStartVals[index] = Binary(false, static_cast<uint8_t>(BinaryQuality::COMM_LOST));
						else
							pConf->BinaryStartVals[index] = Binary(Binaries[n]["StartVal"].asBool(), static_cast<uint8_t>(BinaryQuality::ONLINE));
					}
					else if (pConf->BinaryStartVals.count(index))
						pConf->BinaryStartVals.erase(index);
				}
			}
			std::sort(pConf->BinaryIndicies.begin(), pConf->BinaryIndicies.end());
		}

		if (JSONRoot.isMember("BinaryControls"))
		{
			const auto BinaryControls = JSONRoot["BinaryControls"];
			for (Json::ArrayIndex n = 0; n < BinaryControls.size(); ++n)
			{
				size_t start, stop;
				if (BinaryControls[n].isMember("Index"))
					start = stop = BinaryControls[n]["Index"].asUInt();
				else if (BinaryControls[n]["Range"].isMember("Start") && BinaryControls[n]["Range"].isMember("Stop"))
				{
					start = BinaryControls[n]["Range"]["Start"].asUInt();
					stop = BinaryControls[n]["Range"]["Stop"].asUInt();
				}
				else
				{
					std::cout << "A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '" << BinaryControls[n].toStyledString() << "'" << std::endl;
					start = 1;
					stop = 0;
				}
				for (auto index = start; index <= stop; index++)
				{
					bool exists = false;
					for (auto existing_index : pConf->ControlIndicies)
						if (existing_index == index)
							exists = true;

					if (!exists)
						pConf->ControlIndicies.push_back(index);

					if (BinaryControls[n].isMember("Intervalms"))
						pConf->ControlIntervalms[index] = BinaryControls[n]["Intervalms"].asUInt();

					auto start_val = BinaryControls[n]["StartVal"].asString();
					if (start_val == "D")
					{
						if (pConf->ControlIntervalms.count(index))
							pConf->ControlIntervalms.erase(index);
						for (auto it = pConf->ControlIndicies.begin(); it != pConf->ControlIndicies.end(); it++)
							if (*it == index)
							{
								pConf->ControlIndicies.erase(it);
								break;
							}
					}

					if (BinaryControls[n].isMember("FeedbackBinaries"))
					{
						const auto FeedbackBinaries = BinaryControls[n]["FeedbackBinaries"];
						for (Json::ArrayIndex fbn = 0; fbn < FeedbackBinaries.size(); ++fbn)
						{
							if (!FeedbackBinaries[fbn].isMember("Index"))
							{
								std::cout << "An 'Index' is required for Binary feedback : '" << FeedbackBinaries[fbn].toStyledString() << "'" << std::endl;
								continue;
							}

							BinaryFeedback fb(FeedbackBinaries[fbn]["Index"].asUInt());
							if (FeedbackBinaries[fbn].isMember("OnValue"))
							{
								if (FeedbackBinaries[fbn]["OnValue"].asString() == "X")
									fb.on_value = Binary(false, static_cast<uint8_t>(BinaryQuality::COMM_LOST));
								else
									fb.on_value = Binary(FeedbackBinaries[fbn]["OnValue"].asBool(), static_cast<uint8_t>(BinaryQuality::ONLINE));
							}
							if (FeedbackBinaries[fbn].isMember("OffValue"))
							{
								if (FeedbackBinaries[fbn]["OffValue"].asString() == "X")
									fb.off_value = Binary(false, static_cast<uint8_t>(BinaryQuality::COMM_LOST));
								else
									fb.off_value = Binary(FeedbackBinaries[fbn]["OffValue"].asBool(), static_cast<uint8_t>(BinaryQuality::ONLINE));

							}
							if (FeedbackBinaries[fbn].isMember("FeedbackMode"))
							{
								auto mode = FeedbackBinaries[fbn]["FeedbackMode"].asString();
								if (mode == "PULSE")
									fb.mode = FeedbackMode::PULSE;
								else if (mode == "LATCH")
									fb.mode = FeedbackMode::LATCH;
								else
									std::cout << "Warning: unrecognised feedback mode: '" << FeedbackBinaries[fbn].toStyledString() << "'" << std::endl;
							}
							pConf->ControlFeedback[index].push_back(std::move(fb));
						}
					}
				}
			}
			std::sort(pConf->ControlIndicies.begin(), pConf->ControlIndicies.end());
		}
	}



