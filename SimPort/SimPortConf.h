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

#include <opendatacon/DataPortConf.h>
#include <opendatacon/IOTypes.h>

//DNP3 has 3 control models: complimentary (1-output) latch, complimentary 2-output (pulse), activation (1-output) pulse
//We can generalise, and come up with a simpler superset:
//	-have an arbitrary length list of outputs
//	-arbitrary on/off values for each output
//	-each output either pulsed or latched

typedef enum { PULSE, LATCH } FeedbackMode;
struct BinaryFeedback
{
	size_t binary_index;
	odc::Binary on_value;
	odc::Binary off_value;
	FeedbackMode mode;

	BinaryFeedback(size_t index, odc::Binary on = odc::Binary(true), odc::Binary off = odc::Binary(false), FeedbackMode amode = LATCH):
		binary_index(index),
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

	std::vector<uint32_t> BinaryIndicies;
	std::map<size_t, odc::Binary> BinaryStartVals;
	std::map<size_t, unsigned int> BinaryUpdateIntervalms;
	std::vector<uint32_t> AnalogIndicies;
	std::map<size_t, odc::Analog> AnalogStartVals;
	std::map<size_t, unsigned int> AnalogUpdateIntervalms;
	std::map<size_t, double> AnalogStdDevs;
	std::vector<uint32_t> ControlIndicies;
	std::map<size_t, unsigned int> ControlIntervalms;
	std::map<size_t, std::vector<BinaryFeedback>> ControlFeedback;

	double default_std_dev_factor;
};

#endif // SIMPORTCONF_H
