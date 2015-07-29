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
 * util.cpp
 *
 *  Created on: 14/03/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <opendatacon/util.h>
#include <regex>

using namespace std;
using namespace opendnp3;

bool GetBool(const string& value)
{
	if (value == "true")
	{
		return true;
	}
	else if (value == "false")
	{
		return false;
	}
	throw new std::runtime_error("Expected true or false after element name");
}

bool getline_noncomment(istream& is, string& line)
{
	//chew up blank lines and comments
	do
	{
		std::getline(is, line);
	} while(std::regex_match(line,std::regex("^[:space:]*#.*|^[^_[:alnum:]]*$",std::regex::extended)) && !is.eof());

	//fail if we hit the end
	if(is.eof())
		return false;
	//success!
	return true;
}

bool extract_delimited_string(istream& ist, string& extracted)
{
	extracted.clear();
	char delim;
	//The first char is the delimiter
	if(!(ist>>delim))
		return true; //nothing to extract - return successfully extracted nothing
	char ch;
	while(ist.get(ch))
	{
		//return success once we find the second delimiter
		if(ch == delim)
			return true;
		//otherwise keep extracting
		extracted.push_back(ch);
	}
	//if we get to here, there wasn't a matching end delimiter - return failed
	return false;
}

//bool operator== (OutstationParams& C1, OutstationParams& C2)
//{
//	return (C1.allowUnsolicited == C2.allowUnsolicited)&&
//			(C1.maxControlsPerRequest == C2.maxControlsPerRequest)&&
//			(C1.maxTxFragSize == C2.maxTxFragSize)&&
//			(C1.selectTimeout == C2.selectTimeout)&&
//			(C1.solConfirmTimeout == C2.solConfirmTimeout)&&
//			(C1.unsolClassMask == C2.unsolClassMask)&&
//			(C1.unsolConfirmTimeout == C2.unsolConfirmTimeout)&&
//			(C1.unsolRetryTimeout == C2.unsolRetryTimeout);
//}
//bool operator== (StaticResponseConfig& C1, StaticResponseConfig& C2)
//{
//	return (C1.analog == C2.analog)&&
//			(C1.analogOutputStatus == C2.analogOutputStatus)&&
//			(C1.binary == C2.binary)&&
//			(C1.binaryOutputStatus == C2.binaryOutputStatus)&&
//			(C1.counter == C2.counter)&&
//			(C1.doubleBinary == C2.doubleBinary)&&
//			(C1.frozenCounter == C2.frozenCounter);
//}
//bool operator== (EventResponseConfig& C1, EventResponseConfig& C2)
//{
//	return (C1.analog == C2.analog)&&
//			(C1.analogOutputStatus == C2.analogOutputStatus)&&
//			(C1.binary == C2.binary)&&
//			(C1.binaryOutputStatus == C2.binaryOutputStatus)&&
//			(C1.counter == C2.counter)&&
//			(C1.doubleBinary == C2.doubleBinary)&&
//			(C1.frozenCounter == C2.frozenCounter);
//}
//bool operator== (OutstationConfig& C1, OutstationConfig& C2)
//{
//	return (C1.defaultEventResponses == C2.defaultEventResponses)&&
//			(C1.defaultStaticResponses == C2.defaultStaticResponses)&&
//			(C1.params == C2.params);
//}
//bool operator== (LinkConfig& C1, LinkConfig& C2)
//{
//	return (C1.IsMaster == C2.IsMaster)&&
//			(C1.LocalAddr == C2.LocalAddr)&&
//			(C1.NumRetry == C2.NumRetry)&&
//			(C1.RemoteAddr == C2.RemoteAddr)&&
//			(C1.UseConfirms == C2.UseConfirms)&&
//			(C1.Timeout == C2.Timeout);
//}
//bool operator== (EventBufferConfig& C1, EventBufferConfig& C2)
//{
//	return (C1.maxBinaryEvents == C2.maxBinaryEvents)&&
//			(C1.maxDoubleBinaryEvents == C2.maxDoubleBinaryEvents)&&
//			(C1.maxAnalogEvents == C2.maxAnalogEvents)&&
//			(C1.maxCounterEvents == C2.maxCounterEvents)&&
//			(C1.maxFrozenCounterEvents == C2.maxFrozenCounterEvents)&&
//			(C1.maxBinaryOutputStatusEvents == C2.maxBinaryOutputStatusEvents)&&
//			(C1.maxAnalogOutputStatusEvents == C2.maxAnalogOutputStatusEvents);
//}
//bool operator== (DatabaseTemplate& C1, DatabaseTemplate& C2)
//{
//	return (C1.numBinary == C2.numBinary)&&
//			(C1.numDoubleBinary == C2.numDoubleBinary)&&
//			(C1.numAnalog == C2.numAnalog)&&
//			(C1.numCounter == C2.numCounter)&&
//			(C1.numFrozenCounter == C2.numFrozenCounter)&&
//			(C1.numBinaryOutputStatus == C2.numBinaryOutputStatus)&&
//			(C1.numAnalogOutputStatus == C2.numAnalogOutputStatus);
//}
//bool operator== (OutstationStackConfig& C1, OutstationStackConfig& C2)
//{
//    return (C1.dbTemplate == C2.dbTemplate)&&
//		(C1.eventBuffer == C2.eventBuffer)&&
//		(C1.link == C2.link)&&
//		(C1.outstation == C2.outstation);
//}
//bool operator!= (OutstationStackConfig& C1, OutstationStackConfig& C2)
//{
//    return !(C1 == C2);
//}

