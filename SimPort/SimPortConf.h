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
#include <opendnp3/app/MeasurementTypes.h>
#include <opendnp3/app/ControlRelayOutputBlock.h>

class SimPortConf: public DataPortConf
{
public:
	SimPortConf():
		default_std_dev_factor(0.1)
	{}

	std::vector<uint32_t> BinaryIndicies;
	std::map<size_t, opendnp3::Binary> BinaryStartVals;
	std::map<size_t, unsigned int> BinaryUpdateIntervalms;
	std::vector<uint32_t> AnalogIndicies;
	std::map<size_t, opendnp3::Analog> AnalogStartVals;
	std::map<size_t, unsigned int> AnalogUpdateIntervalms;
	std::map<size_t, double> AnalogStdDevs;
	std::vector<uint32_t> ControlIndicies;
	std::map<size_t, unsigned int> ControlIntervalms;

	double default_std_dev_factor;
};

#endif // SIMPORTCONF_H
