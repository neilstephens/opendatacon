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

#include <opendnp3/outstation/Database.h>

template <typename T>
opendnp3::PointIndexes GetIndexes(const opendnp3::Database& database);

template <>
opendnp3::PointIndexes GetIndexes<opendnp3::Binary>(const opendnp3::Database& database)
{
	return database.staticData.binaries.indexes;
}

template <>
opendnp3::PointIndexes GetIndexes<opendnp3::DoubleBitBinary>(const opendnp3::Database& database)
{
	return database.staticData.doubleBinaries.indexes;
}

template <>
opendnp3::PointIndexes GetIndexes<opendnp3::Analog>(const opendnp3::Database& database)
{
	return database.staticData.analogs.indexes;
}

template <>
opendnp3::PointIndexes GetIndexes<opendnp3::Counter>(const opendnp3::Database& database)
{
	return database.staticData.counters.indexes;
}

template <>
opendnp3::PointIndexes GetIndexes<opendnp3::FrozenCounter>(const opendnp3::Database& database)
{
	return database.staticData.frozenCounters.indexes;
}

template <>
opendnp3::PointIndexes GetIndexes<opendnp3::BinaryOutputStatus>(const opendnp3::Database& database)
{
	return database.staticData.binaryOutputStatii.indexes;
}

template <>
opendnp3::PointIndexes GetIndexes<opendnp3::AnalogOutputStatus>(const opendnp3::Database& database)
{
	return database.staticData.analogOutputStatii.indexes;
}

template<typename T>
T GetCurrentValue(opendnp3::Database& database, uint16_t index)
{
	auto idx = GetIndexes<T>(database).GetPosition(index);
	return database.Values<T>()[idx].current;
}

template<typename T>
T UpdateQuality(opendnp3::Database& database, uint8_t qual, uint16_t index)
{
	T meas(GetCurrentValue<T>(database, index));
	meas.quality = qual;
	return meas;
}

#endif