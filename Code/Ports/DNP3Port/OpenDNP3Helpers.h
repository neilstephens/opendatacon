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
* OpenDNP3Helpers.h
*
*  Created on: 24/11/2014
*      Author: Alan Murray
*/

#ifndef OPENDNP3HELPERS_H_
#define OPENDNP3HELPERS_H_
#include <string>
#include <opendnp3/app/MeasurementTypes.h>
#include <opendnp3/app/MeasurementInfo.h>
#include <opendatacon/IOTypes.h>

opendnp3::StaticBinaryVariation StringToStaticBinaryResponse(const std::string& str);
opendnp3::StaticAnalogVariation StringToStaticAnalogResponse(const std::string& str);
opendnp3::StaticCounterVariation StringToStaticCounterResponse(const std::string& str);
opendnp3::EventBinaryVariation StringToEventBinaryResponse(const std::string& str);
opendnp3::EventAnalogVariation StringToEventAnalogResponse(const std::string& str);
opendnp3::EventCounterVariation StringToEventCounterResponse(const std::string& str);
opendnp3::EventAnalogOutputStatusVariation StringToEventAnalogControlResponse(const std::string& str);
odc::EventType EventAnalogControlResponseToODCEvent(const opendnp3::EventAnalogOutputStatusVariation var);

#endif
