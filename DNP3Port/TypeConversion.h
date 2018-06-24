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
 * DNP3Port/TypeConversion.h
 *
 *  Created on: 24/06/2018
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <opendnp3/app/MeasurementTypes.h>
#include <opendnp3/app/ControlRelayOutputBlock.h>
#include <opendnp3/app/AnalogOutput.h>
#include <opendatacon/IOTypes.h>



namespace odc
{

EventInfo ToODC(opendnp3::Binary& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::DoubleBitBinary& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::Analog& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::Counter& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::FrozenCounter& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::BinaryOutputStatus& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::AnalogOutputStatus& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::BinaryQuality& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::DoubleBitBinaryQuality& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::AnalogQuality& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::CounterQuality& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::BinaryOutputStatusQuality& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::ControlRelayOutputBlock& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::AnalogOutputInt16& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::AnalogOutputInt32& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::AnalogOutputFloat32& dnp3, size_t ind, const std::string& source = "");
EventInfo ToODC(opendnp3::AnalogOutputDouble64& dnp3, size_t ind, const std::string& source = "");

template<typename T> T FromODC(EventInfo event);

} //namespace odc
