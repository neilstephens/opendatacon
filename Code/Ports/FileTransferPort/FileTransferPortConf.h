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
 * FileTransferPortConf.h
 *
 *  Created on: 19/01/2023
 *      Author: Neil Stephens
 */

#ifndef FileTransferPortConf_H_
#define FileTransferPortConf_H_
#include <opendatacon/DataPort.h>
#include <regex>
#include <vector>

using namespace odc;

enum class TransferDirection { TX, RX };
enum class OverwriteMode { OVERWRITE, FAIL, APPEND };
enum class TriggerType { Periodic, BinaryControl, AnalogControl, OctetStringPath };
enum class ModifiedTimePersistence { ONDISK, INMEMORY, PURGEONDISABLE };

struct TransferTrigger
{
	bool OnlyWhenModified = true;
	TriggerType Type = TriggerType::Periodic;
	size_t Periodms = 0;
	size_t Index = 0;
	int32_t Value = 0;
};

template <typename T, typename ... Rest>
void hash_combine(std::size_t& seed, const T& v, const Rest& ... rest)
{
	if constexpr (sizeof(size_t) >= 8)
		seed ^= std::hash<T>{} (v) + 0x517cc1b727220a95 + (seed << 6) + (seed >> 2);
	else
		seed ^= std::hash<T>{} (v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	(hash_combine(seed, rest), ...);
}

namespace std
{
//Provide a specialisation of std::hash for TransferTrigger
// this way we can use it directly as a key for std:: unordered containers
template <> struct hash<TransferTrigger>
{
	std::size_t operator()(const TransferTrigger& key) const noexcept
	{
		std::size_t hash = 0;
		switch(key.Type)
		{
			case TriggerType::Periodic:
				hash_combine(hash, key.Type, key.Periodms);
				break;
			case TriggerType::AnalogControl:
				hash_combine(hash, key.Type, key.Index, key.Value);
				break;
			case TriggerType::BinaryControl:
			case TriggerType::OctetStringPath:
				hash_combine(hash, key.Type, key.Index);
				break;
		}
		return hash;
	}
};
template <> struct equal_to<TransferTrigger>
{
	bool operator()(const TransferTrigger& lhs, const TransferTrigger& rhs) const
	{
		if(lhs.Type != rhs.Type)
			return false;
		switch(lhs.Type)
		{
			case TriggerType::Periodic:
				return lhs.Periodms == rhs.Periodms;
			case TriggerType::AnalogControl:
				return lhs.Index == rhs.Index && lhs.Value == rhs.Value;
			case TriggerType::BinaryControl:
			case TriggerType::OctetStringPath:
				return lhs.Index == rhs.Index;
			default:
				throw runtime_error("Unknown TransferTrigger Type");
		}
	}
};
} //std

struct FilenameConf
{
	std::string InitialName = "UninitialisedFilename.txt";
	std::string Template = "<NAME>";
	std::string DateToken = "";
	std::string DateFormat = "%Y-%m-%d %H:%M:%S.%e";
	std::string EventToken = "<NAME>";
	std::shared_ptr<const EventInfo> Event = nullptr;
};

class FileTransferPortConf: public DataPortConf
{
public:
	TransferDirection Direction = TransferDirection::RX;
	std::string Directory = ".";
	bool Recursive = false;
	std::unique_ptr<std::regex> pFilenameRegex = nullptr;
	size_t FileNameTransmissionMatchGroup = 0;
	std::shared_ptr<const EventInfo> FileNameTransmissionEvent = nullptr;
	size_t SequenceIndexStart = 0;
	size_t SequenceIndexStop = 0;
	int64_t SequenceIndexEOF = -1;
	std::unordered_set<TransferTrigger> TransferTriggers;
	FilenameConf FilenameInfo;
	OverwriteMode Mode = OverwriteMode::FAIL;
	std::chrono::milliseconds ModifiedDwellTimems = std::chrono::milliseconds(500);
	ModifiedTimePersistence Persistence = ModifiedTimePersistence::INMEMORY;
	std::string PersistenceFile = "ModifiedTimePersistence.json";
};

#endif /* FileTransferPortConf_H_ */
