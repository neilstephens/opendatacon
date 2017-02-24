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
 * IndexOffsetTransform.h
 *
 *  Created on: 30/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef INDEXOFFSETTRANSFORM_H_
#define INDEXOFFSETTRANSFORM_H_

#include <cstdint>
#include <opendatacon/Transform.h>

using namespace odc;

class IndexOffsetTransform: public Transform
{
public:
	IndexOffsetTransform(Json::Value params): Transform(params)
	{
		if(params.isMember("Offset") && params["Offset"].isInt())
			offset = params["Offset"].asInt();
		else if(params.isMember("offset") && params["offset"].isInt())
			offset = params["offset"].asInt();
		else
			offset = 0;
	}

	bool Event(Binary& meas, uint16_t& index){return EventT(meas,index);}
	bool Event(DoubleBitBinary& meas, uint16_t& index){return EventT(meas,index);}
	bool Event(Analog& meas, uint16_t& index){return EventT(meas,index);}
	bool Event(Counter& meas, uint16_t& index){return EventT(meas,index);}
	bool Event(FrozenCounter& meas, uint16_t& index){return EventT(meas,index);}
	bool Event(BinaryOutputStatus& meas, uint16_t& index){return EventT(meas,index);}
	bool Event(AnalogOutputStatus& meas, uint16_t& index){return EventT(meas,index);}
	bool Event(ControlRelayOutputBlock& arCommand, uint16_t index){return EventT(arCommand,index);}
	bool Event(AnalogOutputInt16& arCommand, uint16_t index){return EventT(arCommand,index);}
	bool Event(AnalogOutputInt32& arCommand, uint16_t index){return EventT(arCommand,index);}
	bool Event(AnalogOutputFloat32& arCommand, uint16_t index){return EventT(arCommand,index);}
	bool Event(AnalogOutputDouble64& arCommand, uint16_t index){return EventT(arCommand,index);}

	template<typename T> bool EventT(T& meas, uint16_t& index)
	{
		if(offset < (UINT16_MAX - index) && index >= -offset)
		{
			index += offset;
			return true;
		}
		return false;
	}

	int offset;
};

#endif /* INDEXOFFSETTRANSFORM_H_ */
