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

struct TransferTrigger
{
	bool OnlyWhenModified = true;
	TriggerType Type = TriggerType::Periodic;
	size_t Periodms = 0;
	size_t Index = 0;
	int64_t Value = 0;
};

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
	std::vector<TransferTrigger> TransferTriggers;
	FilenameConf FilenameInfo;
	OverwriteMode Mode = OverwriteMode::FAIL;
};

#endif /* FileTransferPortConf_H_ */
