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
 * SimPortConf.h
 *
 *  Created on: 2015-12-16
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef SIMPORTCONF_H
#define SIMPORTCONF_H

#include <memory>
#include <opendatacon/DataPortConf.h>
#include <opendatacon/IOTypes.h>

using namespace odc;

//DNP3 has 3 control models: complimentary (1-output) latch, complimentary 2-output (pulse), activation (1-output) pulse
//We can generalise, and come up with a simpler superset:
//	-have an arbitrary length list of outputs
//	-arbitrary on/off values for each output
//	-each output either pulsed or latched

enum class FeedbackMode { PULSE, LATCH };
struct BinaryFeedback
{
	std::shared_ptr<EventInfo> on_value;
	std::shared_ptr<EventInfo> off_value;
	FeedbackMode mode;

	BinaryFeedback(const std::shared_ptr<EventInfo> on, const std::shared_ptr<EventInfo> off, const FeedbackMode amode):
		on_value(on),
		off_value(off),
		mode(amode)
	{}
};

class SimPortConf: public DataPortConf
{
public:
	SimPortConf():
		default_std_dev_factor(0.1)
	{}

	std::string HttpAddr = "0.0.0.0";
	std::string HttpPort = "";
	std::string Version = "Unknown";
	std::vector<uint32_t> BinaryIndicies;
	std::map<uint32_t, bool> BinaryStartVals;
	std::map<uint32_t, bool> BinaryVals;
	std::map<uint32_t, bool> BinaryForcedStates;
	std::map<uint32_t, unsigned int> BinaryUpdateIntervalms;
	std::vector<uint32_t> AnalogIndicies;
	std::map<uint32_t, double> AnalogStartVals;
	std::map<uint32_t, double> AnalogVals;
	std::map<uint32_t, bool> AnalogForcedStates;
	std::map<uint32_t, unsigned int> AnalogUpdateIntervalms;
	std::map<uint32_t, double> AnalogStdDevs;
	std::vector<uint32_t> ControlIndicies;
	std::map<uint32_t, unsigned int> ControlIntervalms;
	std::map<uint32_t, std::vector<BinaryFeedback>> ControlFeedback;

	double default_std_dev_factor;
};

#endif // SIMPORTCONF_H
