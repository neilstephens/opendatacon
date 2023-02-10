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
 * FileTransferPort.cpp
 *
 *  Created on: 19/01/2023
 *      Author: Neil Stephens
 */

#include "FileTransferPort.h"
#include <opendatacon/util.h>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <fstream>

using namespace odc;

FileTransferPort::FileTransferPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides)
{
	pConf = std::make_unique<FileTransferPortConf>();
	ProcessFile();
}

FileTransferPort::~FileTransferPort()
{
	for(const auto& t : Timers)
		t->cancel();

	//There may be some outstanding handlers
	std::weak_ptr<void> tracker = handler_tracker;
	handler_tracker.reset();

	while(!tracker.expired() && !pIOS->stopped())
		pIOS->poll_one();
}

void FileTransferPort::Enable_()
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	enabled = true;
	if(pConf->Direction == TransferDirection::TX)
	{
		//Setup periodic checks/triggers
		for(const auto& t : pConf->TransferTriggers)
		{
			if(t.Type == TriggerType::Periodic)
			{
				Timers.push_back(asio_service::Get()->make_steady_timer());
				pSyncStrand->dispatch([=,h{handler_tracker}] {Periodic(asio::error_code(),Timers.back(),t.Periodms,t.OnlyWhenModified);});
			}
		}
	}
}
void FileTransferPort::Disable_()
{
	enabled =false;
	for(const auto& t : Timers)
		t->cancel();
	Timers.clear();
}

void FileTransferPort::Periodic(asio::error_code err, std::shared_ptr<asio::steady_timer> pTimer, size_t periodms, bool only_modified)
{
	if(err || !enabled)
		return;

	Tx(only_modified);

	pTimer->expires_from_now(std::chrono::milliseconds(periodms));
	pTimer->async_wait(pSyncStrand->wrap([=,h{handler_tracker}](asio::error_code err)
		{
			Periodic(err,pTimer,periodms,only_modified);
		}));
}

void FileTransferPort::Build()
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	Filename = pConf->FilenameInfo.InitialName;
	seq = pConf->SequenceIndexStart;
	if(pConf->Direction == TransferDirection::RX)
	{
		auto pos = pConf->FilenameInfo.Template.find(pConf->FilenameInfo.EventToken);
		if(pos == pConf->FilenameInfo.Template.npos)
		{
			if(auto log = spdlog::get("FileTransferPort"))
				log->error("{}: Filename event token '{}' not found in template '{}'.", Name, pConf->FilenameInfo.EventToken, pConf->FilenameInfo.Template);
		}
		if(!pConf->FilenameInfo.DateToken.empty())
		{
			pos = pConf->FilenameInfo.Template.find(pConf->FilenameInfo.DateToken);
			if(pos == pConf->FilenameInfo.Template.npos)
			{
				if(auto log = spdlog::get("FileTransferPort"))
					log->error("{}: Filename date token '{}' not found in template '{}'.", Name, pConf->FilenameInfo.DateToken, pConf->FilenameInfo.Template);
			}
		}
	}
}

void FileTransferPort::Event_(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(!enabled)
	{
		if(auto log = spdlog::get("FileTransferPort"))
			log->trace("{}: Disabled - ignoring {} event from {}", Name, ToString(event->GetEventType()), SenderName);
		return;
	}

	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());

	if(pConf->Direction == TransferDirection::TX)
		return TxEvent(event, SenderName, pStatusCallback);

	if(pConf->Direction == TransferDirection::RX)
		return RxEvent(event, SenderName, pStatusCallback);
}

void FileTransferPort::TxEvent(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	//check if the event is a trigger to tx files
	TransferTrigger trig;
	trig.Index = event->GetIndex();
	switch(event->GetEventType())
	{
		case EventType::OctetString:
			trig.Type = TriggerType::OctetStringPath;
			break;
		case EventType::AnalogOutputInt16:
			trig.Type = TriggerType::AnalogControl;
			trig.Value = event->GetPayload<EventType::AnalogOutputInt16>().first;
			break;
		case EventType::AnalogOutputInt32:
			trig.Type = TriggerType::AnalogControl;
			trig.Value = event->GetPayload<EventType::AnalogOutputInt32>().first;
			break;
		case EventType::BinaryCommandEvent:
			trig.Type = TriggerType::BinaryControl;
			break;
		default:
			return (*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
	}

	auto t = pConf->TransferTriggers.find(trig);
	if(t != pConf->TransferTriggers.end())
	{
		if(t->Type == TriggerType::OctetStringPath)
		{
			auto path = ToString(event->GetPayload<EventType::OctetString>(), DataToStringMethod::Raw);
			auto [match,tx_name] = FileNameTransmissionMatch(std::filesystem::path(path).filename().string());
			if(match)
				TxPath(path,tx_name,t->OnlyWhenModified);
			else if(auto log = spdlog::get("FileTransferPort"))
				log->warn("{}: OctetString path event '{}' doesn't match filename regex.", Name, path);
		}
		else
			Tx(t->OnlyWhenModified);
		return (*pStatusCallback)(CommandStatus::SUCCESS);
	}

	return (*pStatusCallback)(CommandStatus::UNDEFINED);
}

void FileTransferPort::RxEvent(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());

	if(event->GetEventType() != EventType::OctetString)
		return (*pStatusCallback)(CommandStatus::NOT_SUPPORTED);

	auto index = event->GetIndex();

	if(static_cast<int64_t>(index) == pConf->SequenceIndexEOF)
	{
		//this is a stand-in for a zero length event reprsenting EOF
		//the payload is the real sequence number in ascii
		try
		{
			index = std::stoul(ToString(event->GetPayload<EventType::OctetString>(), DataToStringMethod::Raw));
		}
		catch(const std::exception& e)
		{
			if(auto log = spdlog::get("FileTransferPort"))
				log->error("{}: OctetString EOF event with unparsable payload: '{}'.", Name, e.what());
			return (*pStatusCallback)(CommandStatus::NOT_SUPPORTED);
		}
		//replace the event with the same thing, but new (sequence number) index and zero-length payload
		EventInfo newevent(event->GetEventType(), index, event->GetSourcePort(), event->GetQuality(), event->GetTimestamp());
		newevent.SetPayload<EventType::OctetString>(OctetStringBuffer());
		event = std::make_shared<const EventInfo>(newevent);
	}

	if(index == pConf->FilenameInfo.Event->GetIndex())
	{
		if(rx_in_progress)
		{
			event_buffer[index].push_back(event);
			return (*pStatusCallback)(CommandStatus::SUCCESS);
		}
		//We need to fill out filename template with event and optionally date
		std::string templated_name = pConf->FilenameInfo.Template;
		auto pos = templated_name.find(pConf->FilenameInfo.EventToken);
		if(pos != templated_name.npos)
		{
			templated_name.erase(pos, pConf->FilenameInfo.EventToken.size());
			templated_name.insert(pos, ToString(event->GetPayload<EventType::OctetString>(), DataToStringMethod::Raw));
		}
		if(!pConf->FilenameInfo.DateToken.empty())
		{
			pos = templated_name.find(pConf->FilenameInfo.DateToken);
			if(pos != templated_name.npos)
			{
				auto date_str = since_epoch_to_datetime(event->GetTimestamp(),pConf->FilenameInfo.DateFormat);
				templated_name.erase(pos, pConf->FilenameInfo.DateToken.size());
				templated_name.insert(pos, date_str);
			}
		}
		Filename = templated_name;
		return (*pStatusCallback)(CommandStatus::SUCCESS);
	}

	if(index >= pConf->SequenceIndexStart && index <= pConf->SequenceIndexStop)
	{
		if(!rx_in_progress)
		{
			rx_in_progress = true;
			seq = pConf->SequenceIndexStart;
			auto path = std::filesystem::path(pConf->Directory) / std::filesystem::path(Filename);
			if(auto log = spdlog::get("FileTransferPort"))
				log->debug("{}: Start RX, writing '{}'.", Name, path.string());
			if(pConf->Mode == OverwriteMode::FAIL && std::filesystem::exists(path))
			{
				if(auto log = spdlog::get("FileTransferPort"))
					log->error("{}: File '{}' already exists and OverwriteMode::FAIL", Name, path.string());
				return (*pStatusCallback)(CommandStatus::BLOCKED);
			}
			std::ios::openmode open_flags = std::ios::binary;
			if(pConf->Mode == OverwriteMode::APPEND)
				open_flags |= std::ios::app;
			fout.open(path,open_flags);
			if(fout.fail())
			{
				if(auto log = spdlog::get("FileTransferPort"))
					log->error("{}: Failed to open file '{}' for writing.", Name, path.string());
				return (*pStatusCallback)(CommandStatus::BLOCKED);
			}
		}

		//push and pop everything through the Q for simplicity - optimise if it pops up in a profile as a hot path
		//TODO: somehow limit the buffer size
		event_buffer[index].push_back(event);
		//call the callback now because we've successfully queued the event
		(*pStatusCallback)(CommandStatus::SUCCESS);

		while(!event_buffer[seq].empty())
		{
			auto popped = event_buffer[seq].front();
			event_buffer[seq].pop_front();
			if(fout)
			{
				auto OSBuffer = popped->GetPayload<EventType::OctetString>();
				if(OSBuffer.size() == 0) //EOF
				{
					fout.close();
					rx_in_progress = false;
					seq = pConf->SequenceIndexStart;
					++FilesTransferred;
					if(auto log = spdlog::get("FileTransferPort"))
						log->debug("{}: Finished writing '{}'.", Name, (std::filesystem::path(pConf->Directory) / std::filesystem::path(Filename)).string());

					//we could already have events from the next file
					auto process_event =
						[&](size_t idx)
						{
							auto popped = event_buffer[idx].front();
							event_buffer[idx].pop_front();
							Event_(popped,SenderName,std::make_shared<std::function<void (CommandStatus)>>([] (CommandStatus){}));
						};
					if(!event_buffer[pConf->FilenameInfo.Event->GetIndex()].empty())
						process_event(pConf->FilenameInfo.Event->GetIndex());
					if(!event_buffer[seq].empty())
						process_event(seq);
					return;
				}
				if(!fout.write(static_cast<const char*>(OSBuffer.data()),OSBuffer.size()))
				{
					if(auto log = spdlog::get("FileTransferPort"))
						log->error("{}: Mid-RX writing failed on '{}'.", Name, (std::filesystem::path(pConf->Directory) / std::filesystem::path(Filename)).string());
				}
				FileBytesTransferred += OSBuffer.size();
				++seq;
			}
			//else - we should have already logged an error when fout went bad.
		}
		return;
	}

	//it's not a filename, or file data at this point
	return (*pStatusCallback)(CommandStatus::UNDEFINED);
}

void FileTransferPort::TxPath(std::string path, std::string tx_name, bool only_modified)
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());

	//convert path to a standard format
	auto base_path = std::filesystem::path(pConf->Directory);
	auto std_path = std::filesystem::path(path);
	if(std_path.is_relative())
		std_path = base_path/std_path;
	std_path = std::filesystem::weakly_canonical(std_path);

	//now make sure it's a subdirectory of our configered dir
	auto rel_path = std::filesystem::relative(std_path,base_path);
	if(rel_path.string() == "" || rel_path.string().substr(0,2) == "..")
	{
		if(auto log = spdlog::get("FileTransferPort"))
			log->warn("{}: Requested path not in transfer directory: '{}'", Name, std_path.string());
		return;
	}

	path = std_path.string();

	if(!std::filesystem::exists(path))
	{
		if(auto log = spdlog::get("FileTransferPort"))
			log->warn("{}: File not found: '{}'", Name, path);
		FileModTimes.erase(path);
		return;
	}

	if(only_modified)
	{
		auto updated_time = std::filesystem::last_write_time(path);
		auto last_update_it = FileModTimes.find(path);
		if(last_update_it != FileModTimes.end())
		{
			if(updated_time > last_update_it->second)
				FileModTimes[path] = updated_time;
			else //not modified
				return;
		}
		else //it's new
			FileModTimes[path] = updated_time;
	}

	if(!tx_name.empty())
	{
		auto name_event = std::make_shared<EventInfo>(*pConf->FileNameTransmissionEvent);
		name_event->SetTimestamp();
		name_event->SetPayload<EventType::OctetString>(OctetStringBuffer(std::move(tx_name)));
		std::atomic_bool recv(false);
		PublishEvent(name_event,std::make_shared<std::function<void (CommandStatus)>>([&recv](CommandStatus){recv = true;}));
		//TODO: make a timeout or an option to not sync
		while(!recv)
			pIOS->poll_one();
	}

	std::ifstream fin(path, std::ios::binary);
	if (fin.fail())
	{
		if(auto log = spdlog::get("FileTransferPort"))
			log->error("{}: Failed to open file path for reading: '{}'",Name,path);
		return;
	}

	while(fin)
	{
		auto file_data_chunk = std::vector<char>(255);
		fin.read(file_data_chunk.data(),255);
		auto data_size = fin.gcount();
		if(data_size == 0)
			break;
		if(data_size < 255)
			file_data_chunk.resize(data_size);

		auto chunk_event = std::make_shared<EventInfo>(EventType::OctetString, seq, Name);
		chunk_event->SetPayload<EventType::OctetString>(OctetStringBuffer(std::move(file_data_chunk)));
		PublishEvent(chunk_event);
		if(++seq > pConf->SequenceIndexStop) seq = pConf->SequenceIndexStart;
		FileBytesTransferred += data_size;
	}
	fin.close();

	if(pConf->SequenceIndexEOF >= 0)
	{
		//Send special OctetString as EOF
		auto eof_event = std::make_shared<EventInfo>(EventType::OctetString, pConf->SequenceIndexEOF, Name);
		eof_event->SetPayload<EventType::OctetString>(OctetStringBuffer(std::to_string(seq)));
		PublishEvent(eof_event);
	}
	else
	{
		//Send an empty OctetString as EOF
		auto eof_event = std::make_shared<EventInfo>(EventType::OctetString, seq, Name);
		eof_event->SetPayload<EventType::OctetString>(OctetStringBuffer());
		PublishEvent(eof_event);
	}
	seq = pConf->SequenceIndexStart;
	++FilesTransferred;
}

void FileTransferPort::Tx(bool only_modified)
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	size_t dir_count = 0, match_count = 0;
	auto file_visit = [&](const auto& file)
				{
					++dir_count;
					auto [match,tx_name] = FileNameTransmissionMatch(file.path().filename().string());
					if(match)
					{
						++match_count;
						TxPath(file.path().string(), tx_name, only_modified);
					}
				};
	if(pConf->Recursive)
	{
		for(const auto& file : std::filesystem::recursive_directory_iterator(pConf->Directory))
			file_visit(file);
	}
	else
	{
		for(const auto& file : std::filesystem::directory_iterator(pConf->Directory))
			file_visit(file);
	}

	TxFileDirCount = dir_count;
	TxFileMatchCount = match_count;
}

std::pair<bool,std::string> FileTransferPort::FileNameTransmissionMatch(const std::string& filename)
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	std::smatch match_results;
	if(std::regex_match(filename, match_results, *pConf->pFilenameRegex))
	{
		std::string tx_name = "";
		if(pConf->FileNameTransmissionEvent)
		{
			if(pConf->FileNameTransmissionMatchGroup > match_results.size())
				tx_name = match_results.position(pConf->FileNameTransmissionMatchGroup);
			else
				tx_name = filename;
		}
		return {true,tx_name};
	}
	return {false,""};
}

void FileTransferPort::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	auto log = spdlog::get("FileTransferPort");
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());

	if(JSONRoot.isMember("Direction"))
	{
		const auto dir = JSONRoot["Direction"].asString();
		if(dir == "RX")
			pConf->Direction = TransferDirection::RX;
		else if(dir == "TX")
			pConf->Direction = TransferDirection::TX;
		else if(log)
			log->error("{}: Direction should be either 'Rx' or 'Tx', not '{}'", Name, dir);
	}
	if(JSONRoot.isMember("Directory"))
	{
		pConf->Directory = JSONRoot["Directory"].asString();
		try
		{
			pConf->Directory = std::filesystem::canonical(pConf->Directory).string();
		}
		catch(const std::exception& ex)
		{
			auto default_path = std::filesystem::current_path().string();
			if(log) log->error("{}: Problem using '{}' as directory: '{}'. Using {}", Name, pConf->Directory, ex.what(), default_path);
			pConf->Directory = default_path;
		}
	}
	if(JSONRoot.isMember("FilenameRegex"))
	{
		auto regx_string = JSONRoot["FilenameRegex"].asString();
		try
		{
			pConf->pFilenameRegex = std::make_unique<std::regex>(regx_string,std::regex::extended);
		}
		catch (std::exception& e)
		{
			if(log) log->error("{}: Problem using '{}' as regex: {}", Name, regx_string,e.what());
			pConf->pFilenameRegex = nullptr;
		}
	}
	if(JSONRoot.isMember("Recursive"))
	{
		pConf->Recursive = JSONRoot["Recursive"].asBool();
	}
	if(JSONRoot.isMember("FileNameTransmission"))
	{
		Json::Value jFNTX = JSONRoot["FileNameTransmission"];
		if(jFNTX.isMember("Index") && jFNTX.isMember("MatchGroup"))
		{
			pConf->FileNameTransmissionEvent = std::make_shared<EventInfo>(EventType::OctetString, jFNTX["Index"].asUInt(), Name);
			pConf->FileNameTransmissionMatchGroup = jFNTX["MatchGroup"].asUInt();
		}
		else if(log)
			log->error("{}: FileNameTransmission needs 'Index' and 'MatchGroup' configuration members. Got: {}", Name, jFNTX.toStyledString());
	}
	if(JSONRoot.isMember("SequenceIndexRange"))
	{
		Json::Value jSIR = JSONRoot["SequenceIndexRange"];
		if(jSIR.isMember("Start") && jSIR.isMember("Stop"))
		{
			pConf->SequenceIndexStart = jSIR["Start"].asUInt();
			pConf->SequenceIndexStop = jSIR["Stop"].asUInt();
			if(jSIR.isMember("EOF"))
				pConf->SequenceIndexEOF = jSIR["EOF"].asUInt();
		}
		else if(log)
			log->error("{}: SequenceIndexRange needs 'Start' and 'Stop' configuration members. Got: {}", Name, jSIR.toStyledString());
	}
	if(JSONRoot.isMember("TransferTriggers"))
	{
		auto jTXTrigss = JSONRoot["TransferTriggers"];
		if(jTXTrigss.isArray())
		{
			for(auto jTXTrig : jTXTrigss)
			{
				if(jTXTrig.isMember("Type"))
				{
					TransferTrigger trig;
					const auto typ = jTXTrig["Type"].asString();
					if(typ == "Periodic")
					{
						if(jTXTrig.isMember("Periodms"))
						{
							trig.Type = TriggerType::Periodic;
							trig.Periodms = jTXTrig["Periodms"].asUInt();
						}
						else
						{
							if(log)
								log->error("{}: A TransferTrigger of Type Periodic needs 'Periodms'. Got: {}", Name, jTXTrig.toStyledString());
							continue;
						}
					}
					else if(typ == "BinaryControl")
					{
						if(jTXTrig.isMember("Index"))
						{
							trig.Type = TriggerType::BinaryControl;
							trig.Index = jTXTrig["Index"].asUInt();
						}
						else
						{
							if(log)
								log->error("{}: A TransferTrigger of Type BinaryControl needs an 'Index'. Got: {}", Name, jTXTrig.toStyledString());
							continue;
						}
					}
					else if(typ == "AnalogControl")
					{
						if(jTXTrig.isMember("Index") && jTXTrig.isMember("Value"))
						{
							trig.Type = TriggerType::AnalogControl;
							trig.Index = jTXTrig["Index"].asUInt();
							trig.Value = jTXTrig["Value"].asInt64();
						}
						else
						{
							if(log)
								log->error("{}: A TransferTrigger of Type BinaryControl needs an 'Index'. Got: {}", Name, jTXTrig.toStyledString());
							continue;
						}
					}
					else if(typ == "OctetStringPath")
					{
						if(jTXTrig.isMember("Index"))
						{
							trig.Type = TriggerType::OctetStringPath;
							trig.Index = jTXTrig["Index"].asUInt();
						}
						else
						{
							if(log)
								log->error("{}: A TransferTrigger of Type OctetStringPath needs an 'Index'. Got: {}", Name, jTXTrig.toStyledString());
							continue;
						}
					}
					else
					{
						if(log)
							log->error("{}: Invalid TransferTrigger Type '{}'", Name, typ);
						continue;
					}
					if(jTXTrig.isMember("OnlyWhenModified"))
					{
						trig.OnlyWhenModified = jTXTrig["OnlyWhenModified"].asBool();
					}
					pConf->TransferTriggers.insert(trig);
				}
				else if(log)
					log->error("{}: A TransferTrigger needs a 'Type'. Got: {}", Name, jTXTrig.toStyledString());
			}
		}
		else if (log)
			log->error("{}: TransferTriggers is meant to be a JSON array, got {}", Name, jTXTrigss.toStyledString());

	}
	if(JSONRoot.isMember("Filename"))
	{
		const auto jFN = JSONRoot["Filename"];
		if(jFN.isMember("InitialName"))
		{
			pConf->FilenameInfo.InitialName = jFN["InitialName"].asString();
		}
		if(jFN.isMember("Template"))
		{
			pConf->FilenameInfo.Template = jFN["Template"].asString();
		}
		if(jFN.isMember("Date"))
		{
			if(jFN["Date"].isMember("Token"))
			{
				pConf->FilenameInfo.DateToken = jFN["Date"]["Token"].asString();
				if(jFN["Date"].isMember("Format"))
					pConf->FilenameInfo.DateFormat = jFN["Date"]["Format"].asString();
			}
			else if (log)
				log->error("{}: Filename Date option needs 'Token', got {}", Name, jFN["Date"].toStyledString());
		}
		if(jFN.isMember("Event"))
		{
			if(jFN["Event"].isMember("Index"))
			{
				pConf->FilenameInfo.Event = std::make_shared<const EventInfo>(EventType::OctetString, jFN["Event"]["Index"].asUInt());
				if(jFN["Event"].isMember("Token"))
					pConf->FilenameInfo.EventToken = jFN["Event"]["Token"].asString();
			}
			else if (log)
				log->error("{}: Filename Event option needs 'Index', got {}", Name, jFN["Event"].toStyledString());
		}
	}
	if(JSONRoot.isMember("OverwriteMode"))
	{
		const auto modestr = JSONRoot["OverwriteMode"].asString();
		if(modestr == "OVERWRITE")
			pConf->Mode = OverwriteMode::OVERWRITE;
		else if(modestr == "APPEND")
			pConf->Mode = OverwriteMode::APPEND;
		else if(modestr == "FAIL")
			pConf->Mode = OverwriteMode::FAIL;
		else if (log)
			log->error("{}: Invalid OverwriteMode '{}'", Name, modestr);
	}
}

const Json::Value FileTransferPort::GetStatistics() const
{
	Json::Value ret;
	ret["FilesTransferred"] = size_t(FilesTransferred);
	ret["FileBytesTransferred"] = size_t(FileBytesTransferred);
	ret["TxFileDirCount"] = size_t(TxFileDirCount);
	ret["TxFileMatchCount"] = size_t(TxFileMatchCount);
	return ret;
}
