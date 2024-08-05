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
#include "OpenDNP3Helpers.h"
#include <opendnp3/app/MeasurementTypes.h>
#include <stdexcept>

opendnp3::StaticBinaryVariation StringToStaticBinaryResponse(const std::string& str)
{
	if (str == "Group1Var1") return opendnp3::StaticBinaryVariation::Group1Var1;
	if (str == "Group1Var2") return opendnp3::StaticBinaryVariation::Group1Var2;
	throw std::runtime_error("Unknown Static binary response type");
}

opendnp3::StaticAnalogVariation StringToStaticAnalogResponse(const std::string& str)
{
	if (str == "Group30Var1") return opendnp3::StaticAnalogVariation::Group30Var1;
	if (str == "Group30Var2") return opendnp3::StaticAnalogVariation::Group30Var2;
	if (str == "Group30Var3") return opendnp3::StaticAnalogVariation::Group30Var3;
	if (str == "Group30Var4") return opendnp3::StaticAnalogVariation::Group30Var4;
	if (str == "Group30Var5") return opendnp3::StaticAnalogVariation::Group30Var5;
	if (str == "Group30Var6") return opendnp3::StaticAnalogVariation::Group30Var6;
	throw std::runtime_error("Unknown Static analog response type");
}

opendnp3::StaticCounterVariation StringToStaticCounterResponse(const std::string& str)
{
	if (str == "Group20Var1") return opendnp3::StaticCounterVariation::Group20Var1;
	if (str == "Group20Var2") return opendnp3::StaticCounterVariation::Group20Var2;
	if (str == "Group20Var5") return opendnp3::StaticCounterVariation::Group20Var5;
	if (str == "Group20Var6") return opendnp3::StaticCounterVariation::Group20Var6;
	throw std::runtime_error("Unknown Static counter response type");
}

opendnp3::StaticAnalogOutputStatusVariation StringToStaticAnalogOutputStatusResponse(const std::string& str)
{
	if (str == "Group40Var1") return opendnp3::StaticAnalogOutputStatusVariation::Group40Var1;
	if (str == "Group40Var2") return opendnp3::StaticAnalogOutputStatusVariation::Group40Var2;
	if (str == "Group40Var3") return opendnp3::StaticAnalogOutputStatusVariation::Group40Var3;
	if (str == "Group40Var4") return opendnp3::StaticAnalogOutputStatusVariation::Group40Var4;
	throw std::runtime_error("Unknown Static analog output status response type");
}

opendnp3::StaticBinaryOutputStatusVariation StringToStaticBinaryOutputStatusResponse(const std::string& str)
{
	if (str == "Group10Var2") return opendnp3::StaticBinaryOutputStatusVariation::Group10Var2;
	throw std::runtime_error("Unknown Static Binary Output Status response type");
}

opendnp3::EventBinaryVariation StringToEventBinaryResponse(const std::string& str)
{
	if (str == "Group2Var1") return opendnp3::EventBinaryVariation::Group2Var1;
	if (str == "Group2Var2") return opendnp3::EventBinaryVariation::Group2Var2;
	throw std::runtime_error("Unknown event binary response type");
}

opendnp3::EventAnalogVariation StringToEventAnalogResponse(const std::string& str)
{
	if (str == "Group32Var1") return opendnp3::EventAnalogVariation::Group32Var1;
	if (str == "Group32Var2") return opendnp3::EventAnalogVariation::Group32Var2;
	if (str == "Group32Var3") return opendnp3::EventAnalogVariation::Group32Var3;
	if (str == "Group32Var4") return opendnp3::EventAnalogVariation::Group32Var4;
	if (str == "Group32Var5") return opendnp3::EventAnalogVariation::Group32Var5;
	if (str == "Group32Var6") return opendnp3::EventAnalogVariation::Group32Var6;
	if (str == "Group32Var7") return opendnp3::EventAnalogVariation::Group32Var7;
	if (str == "Group32Var8") return opendnp3::EventAnalogVariation::Group32Var8;
	throw std::runtime_error("Unknown event analog response type");
}

opendnp3::EventCounterVariation StringToEventCounterResponse(const std::string& str)
{
	if (str == "Group22Var1") return opendnp3::EventCounterVariation::Group22Var1;
	if (str == "Group22Var2") return opendnp3::EventCounterVariation::Group22Var2;
	if (str == "Group22Var5") return opendnp3::EventCounterVariation::Group22Var5;
	if (str == "Group22Var6") return opendnp3::EventCounterVariation::Group22Var6;
	throw std::runtime_error("Unknown event counter response type");
}

opendnp3::EventBinaryOutputStatusVariation StringToEventBinaryOutputStatusResponse(const std::string& str)
{
	if (str == "Group11Var1") return opendnp3::EventBinaryOutputStatusVariation::Group11Var1;
	if (str == "Group11Var2") return opendnp3::EventBinaryOutputStatusVariation::Group11Var2;
	throw std::runtime_error("Unknown event binary output status response type");
}

opendnp3::EventAnalogOutputStatusVariation StringToEventAnalogOutputStatusResponse(const std::string& str)
{
	if (str == "Group42Var1") return opendnp3::EventAnalogOutputStatusVariation::Group42Var1;
	if (str == "Group42Var2") return opendnp3::EventAnalogOutputStatusVariation::Group42Var2;
	if (str == "Group42Var3") return opendnp3::EventAnalogOutputStatusVariation::Group42Var3;
	if (str == "Group42Var4") return opendnp3::EventAnalogOutputStatusVariation::Group42Var4;
	if (str == "Group42Var5") return opendnp3::EventAnalogOutputStatusVariation::Group42Var5;
	if (str == "Group42Var6") return opendnp3::EventAnalogOutputStatusVariation::Group42Var6;
	if (str == "Group42Var7") return opendnp3::EventAnalogOutputStatusVariation::Group42Var7;
	if (str == "Group42Var8") return opendnp3::EventAnalogOutputStatusVariation::Group42Var8;
	throw std::runtime_error("Unknown event analog output status response type");
}
