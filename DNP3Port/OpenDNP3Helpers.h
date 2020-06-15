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
#include <openpal/container/ArrayView.h>

/*
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
T GetCurrentValue(opendnp3::IDatabase& database, uint16_t index)
{
        database.Modify()
        auto idx = GetIndexes<T>(database).GetPosition(index);
        return database.Values<T>()[idx].current;
}

template<typename T>
T UpdateQuality(opendnp3::IDatabase& database, uint8_t qual, uint16_t index)
{
        T meas(GetCurrentValue<T>(database, index));
        meas.quality = qual;
        return meas;
}*/

opendnp3::StaticBinaryVariation StringToStaticBinaryResponse(const std::string& str);
opendnp3::StaticAnalogVariation StringToStaticAnalogResponse(const std::string& str);
opendnp3::StaticCounterVariation StringToStaticCounterResponse(const std::string& str);
opendnp3::EventBinaryVariation StringToEventBinaryResponse(const std::string& str);
opendnp3::EventAnalogVariation StringToEventAnalogResponse(const std::string& str);
opendnp3::EventCounterVariation StringToEventCounterResponse(const std::string& str);

template <class ValueType, class IndexType>
class ArrayViewIterator
{
public:
	ArrayViewIterator(openpal::ArrayView<ValueType, IndexType>* data, IndexType pos)
		: _pos(pos)
		, _data(data)
	{ }

	bool
	operator!= (const ArrayViewIterator<ValueType, IndexType>& other) const
	{
		return _pos != other._pos;
	}

	ValueType& operator* () const
	{
		return (*_data)[_pos];
	}

	const ArrayViewIterator& operator++ ()
	{
		++_pos;
		// although not strictly necessary for a range-based for loop
		// following the normal convention of returning a value from
		// operator++ is a good idea.
		return *this;
	}

private:
	IndexType _pos;
	openpal::ArrayView<ValueType, IndexType>* _data;
};

namespace openpal
{
template <class ValueType, class IndexType>
ArrayViewIterator<ValueType, IndexType> begin(openpal::ArrayView<ValueType, IndexType>& data)
{
	return ArrayViewIterator<ValueType, IndexType>(&data, 0);
}

template <class ValueType, class IndexType>
ArrayViewIterator<ValueType, IndexType> end(openpal::ArrayView<ValueType, IndexType>& data)
{
	return ArrayViewIterator<ValueType, IndexType>(&data, data.Size());
}
}

#endif
