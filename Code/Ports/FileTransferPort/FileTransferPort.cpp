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
#include <exception>

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

	//ConfirmHandler might be holding a copy of handler_tracker
	ConfirmHandler = [] {};

	//There may be some outstanding handlers
	std::weak_ptr<void> tracker = handler_tracker;
	handler_tracker.reset();

	while(!tracker.expired() && !pIOS->stopped())
		pIOS->poll_one();
}

//called on strand by Disable_()
void FileTransferPort::SaveModTimes()
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());

	std::ofstream fout(pConf->PersistenceFile);
	if(fout.fail())
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->error("{}: Failed to write PersistenceFile '{}'.", Name, pConf->PersistenceFile);
		return;
	}

	Json::Value JsonModTimes;
	for(const auto& [path,time] : FileModTimes)
	{
		auto sys_time = fs_to_sys_time_point(time);
		auto msec_duration = std::chrono::ceil<std::chrono::milliseconds>(sys_time.time_since_epoch());
		JsonModTimes["FileModTimes"][path] = since_epoch_to_datetime(msec_duration.count());
	}

	fout<<JsonModTimes.toStyledString();
	fout.flush();
	fout.close();
}

//called on strand by Enable_()
void FileTransferPort::LoadModTimes()
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());

	std::ifstream fin(pConf->PersistenceFile);
	if(fin.fail())
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->debug("{}: Failed to read PersistenceFile '{}'.", Name, pConf->PersistenceFile);
		return;
	}

	Json::CharReaderBuilder JSONReader;
	std::string err_str;
	Json::Value JsonModTimes;
	bool parse_success = Json::parseFromStream(JSONReader,fin, &JsonModTimes, &err_str);
	fin.close();

	if(!parse_success)
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->error("{}: Failed to parse PersistenceFile '{}': '{}'", Name, pConf->PersistenceFile, err_str);
		return;
	}
	if(!JsonModTimes.isMember("FileModTimes"))
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->error("{}: Failed to parse PersistenceFile '{}': No member 'FileModTimes'", Name, pConf->PersistenceFile);
		return;
	}
	auto filenames = JsonModTimes["FileModTimes"].getMemberNames();
	for(const auto& filename : filenames)
	{
		auto date_str = JsonModTimes["FileModTimes"][filename].asString();
		msSinceEpoch_t ms_since_epoch;
		try
		{
			ms_since_epoch = datetime_to_since_epoch(date_str);
		}
		catch(const std::exception& e)
		{
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: Error parsing mod time '{}' for '{}': '{}'", Name, date_str, filename, e.what());
			continue;
		}
		FileModTimes[filename] = std::filesystem::file_time_type(std::chrono::milliseconds(ms_since_epoch));
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->debug("{}: Raw/Parsed mod time '{}'/'{}' for '{}'.", Name, date_str, since_epoch_to_datetime(ms_since_epoch), filename);
	}
}

//posted on strand by Enable()
void FileTransferPort::Enable_()
{
	if(auto log = odc::spdlog_get("FileTransferPort"))
		log->debug("{}: Enable_().", Name);

	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	enabled = true;
	tx_in_progress = false;
	transfer_reset = false;
	PublishEvent(ConnectState::PORT_UP);
	PublishEvent(ConnectState::CONNECTED);

	if(pConf->Persistence == ModifiedTimePersistence::ONDISK && tx_filename_q.empty())
		LoadModTimes();

	if(pConf->Direction == TransferDirection::TX)
	{
		//Setup periodic checks/triggers
		for(const auto& t : pConf->TransferTriggers)
		{
			if(t.Type == TriggerType::Periodic)
			{
				Timers.push_back(asio_service::Get()->make_steady_timer());
				Periodic(asio::error_code(),Timers.back(),t.Periodms,t.OnlyWhenModified);
			}
		}
	}
	pIdleTimer->expires_from_now(std::chrono::milliseconds(pConf->SequenceResetIdleTimems));
	pIdleTimer->async_wait(pSyncStrand->wrap([this,h{handler_tracker}](asio::error_code err){ if(!err) ResetTransfer();}));
	if(!tx_filename_q.empty())
		pSyncStrand->post([this,h{handler_tracker},next{*tx_filename_q.begin()}]
			{
				auto& [next_path,next_tx_name] = next;
				TrySend(next_path, next_tx_name);
			});
}

//posted on strand by Disable()
void FileTransferPort::Disable_()
{
	if(auto log = odc::spdlog_get("FileTransferPort"))
		log->debug("{}: Disable_().", Name);

	ResetTransfer();

	enabled = false;
	pThrottleTimer->cancel();
	pTransferTimeoutTimer->cancel();
	pIdleTimer->cancel();
	pConfirmTimer->cancel();
	for(const auto& t : Timers)
		t->cancel();
	Timers.clear();

	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	if(pConf->Persistence == ModifiedTimePersistence::PURGEONDISABLE)
		FileModTimes.clear();
	//only persist to disk if the filename Q is empty, otherwise half-finished or Q'd transfers won't happen on re-start
	else if(pConf->Persistence == ModifiedTimePersistence::ONDISK && tx_filename_q.empty())
		SaveModTimes();

	PublishEvent(ConnectState::PORT_DOWN);
	PublishEvent(ConnectState::DISCONNECTED);
}

//called on strand by Enable_(), then wrapped on strand as timer callback
void FileTransferPort::Periodic(asio::error_code err, std::shared_ptr<asio::steady_timer> pTimer, size_t periodms, bool only_modified)
{
	if(err || !enabled)
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->debug("{}: Cancelling periodic trigger: '{}'.", Name, err.message());
		return;
	}

	if(InDemand())
		Tx(only_modified);

	pTimer->expires_from_now(std::chrono::milliseconds(periodms));
	pTimer->async_wait(pSyncStrand->wrap([=,h{handler_tracker}](asio::error_code err)
		{
			Periodic(err,pTimer,periodms,only_modified);
		}));
}

//called off strand by DataConcentrator while ASIO blocked
void FileTransferPort::Build()
{
	if(auto log = odc::spdlog_get("FileTransferPort"))
		log->debug("{}: Build().", Name);
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	crc_size = pConf->UseCRCs ? sizeof(crc) : 0;
	Filename = pConf->FilenameInfo.InitialName;
	seq = pConf->SequenceIndexStart;
	if(pConf->Direction == TransferDirection::RX)
	{
		auto pos = pConf->FilenameInfo.Template.find(pConf->FilenameInfo.EventToken);
		if(pos == pConf->FilenameInfo.Template.npos)
		{
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: Filename event token '{}' not found in template '{}'.", Name, pConf->FilenameInfo.EventToken, pConf->FilenameInfo.Template);
		}
		if(!pConf->FilenameInfo.DateToken.empty())
		{
			pos = pConf->FilenameInfo.Template.find(pConf->FilenameInfo.DateToken);
			if(pos == pConf->FilenameInfo.Template.npos)
			{
				if(auto log = odc::spdlog_get("FileTransferPort"))
					log->error("{}: Filename date token '{}' not found in template '{}'.", Name, pConf->FilenameInfo.DateToken, pConf->FilenameInfo.Template);
			}
		}
	}
	else //TX
	{
		if(!pConf->FileNameTransmissionEvent)
		{
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: 'FileNameTransmission' config not set. Defaulting to index 0.", Name);
			pConf->FileNameTransmissionEvent = std::make_shared<EventInfo>(EventType::OctetString, 0, Name);
		}
		auto& fn_idx = pConf->FileNameTransmissionEvent->GetIndex();

		if(pConf->UseCRCs && pConf->SequenceIndexEOF >= 0)
		{
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->warn("{}: SequenceIndexRange EOF not used becauee UseCRCs == true.", Name);
			pConf->SequenceIndexEOF = -1;
		}

		std::string msg = "";
		if(pConf->SequenceIndexStart > pConf->SequenceIndexStop)
		{
			msg += "SequenceIndexRange, Start > Stop. ";
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: {}", Name, msg);
		}
		if(fn_idx >= pConf->SequenceIndexStart && fn_idx <= pConf->SequenceIndexStop)
		{
			msg += "FileNameTransmission Index clashes with SequenceIndexRange. ";
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: {}", Name, msg);
		}
		if(pConf->SequenceIndexEOF >= (int64_t)pConf->SequenceIndexStart && pConf->SequenceIndexEOF <= (int64_t)pConf->SequenceIndexStop)
		{
			msg += "SequenceIndexRange EOF clashes with SequenceIndexRange Start/Stop. ";
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: {}", Name, msg);
		}
		if(fn_idx >= pConf->SequenceIndexStart && fn_idx <= pConf->SequenceIndexStop)
		{
			msg += "FileNameTransmission Index clashes with SequenceIndexRange.";
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: {}", Name, msg);
		}
		if(msg != "")
			throw std::invalid_argument(msg);
	}

	if(pConf->UseConfirms && pConf->ConfirmControlIndex < 0)
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->error("{}: UseConfirms == true, but no ConfirmControlIndex set. Defaulting to index 0.", Name);
		pConf->ConfirmControlIndex = 0;
	}
}

//posted on strand by Event(), or called recursively from RxEvent()
void FileTransferPort::Event_(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(!enabled)
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->trace("{}: Disabled - ignoring {} event from {}", Name, ToString(event->GetEventType()), SenderName);
		pIOS->post([=] { (*pStatusCallback)(CommandStatus::UNDEFINED); });
		return;
	}

	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());

	if(pConf->Direction == TransferDirection::TX)
		return TxEvent(event, SenderName, pStatusCallback);

	if(pConf->Direction == TransferDirection::RX)
		return RxEvent(event, SenderName, pStatusCallback);
}

//called on strand by Event_()
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
		case EventType::ControlRelayOutputBlock:
			trig.Type = TriggerType::BinaryControl;
			break;
		default:
			pIOS->post([=] { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); });
			return;
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
			else if(auto log = odc::spdlog_get("FileTransferPort"))
				log->warn("{}: OctetString path event '{}' doesn't match filename regex.", Name, path);
		}
		else
			Tx(t->OnlyWhenModified);
		pIOS->post([=] { (*pStatusCallback)(CommandStatus::SUCCESS); });
		return;
	}
	else if(event->GetEventType() == EventType::ControlRelayOutputBlock && (int64_t)event->GetIndex() == pConf->ConfirmControlIndex)
	{
		return ConfirmEvent(event, SenderName, pStatusCallback);
	}

	pIOS->post([=] { (*pStatusCallback)(CommandStatus::UNDEFINED); });
	return;
}

//called on-strand by Event_()
void FileTransferPort::ConfirmEvent(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	auto CROB = event->GetPayload<EventType::ControlRelayOutputBlock>();
	if(CROB.status == CommandStatus::SUCCESS)
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->trace("{}: Received positive confirmation: Entire sequence OK.", Name);
		pConfirmTimer->cancel();
		ConfirmHandler();
		ConfirmHandler = [] {};
		tx_event_buffer.clear();
		tx_uncofirmed_transfers.clear();
	}
	else if(CROB.status == CommandStatus::TIMEOUT)
	{
		const auto& expected_sequence = CROB.onTimeMS;
		const auto& expected_crc = CROB.offTimeMS;
		const bool UnsolConfirm = (CROB.count == 2);

		auto crc_str = pConf->UseCRCs ? fmt::format("0x{:04x}", expected_crc) : "NOT_USED";
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->debug("{}: Received negative confirmation: Send again from sequence {}, CRC {}", Name, expected_sequence, crc_str);

		if(pConf->UseCRCs)
			ResendFrom(expected_sequence, expected_crc);
		else
			ResendFrom(expected_sequence);

		if(!UnsolConfirm && !transfer_reset)
			StartConfirmTimer();
	}
	else if(auto log = odc::spdlog_get("FileTransferPort"))
		log->error("{}: Ignoring confirm event with unexpected status: '{}'", Name, ToString(CROB.status));

	if(!tx_in_progress && !tx_filename_q.empty())
		pSyncStrand->post([this,h{handler_tracker},next{*tx_filename_q.begin()}]
			{
				auto& [next_path,next_tx_name] = next;
				TrySend(next_path, next_tx_name);
			});

	pIOS->post([=] { (*pStatusCallback)(CommandStatus::UNDEFINED); });
	return;
}

//called on strand by Event_()
void FileTransferPort::RxEvent(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());

	if(event->GetEventType() != EventType::OctetString)
		return pIOS->post([=] { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); });

	auto index = event->GetIndex();

	if(static_cast<int64_t>(index) == pConf->SequenceIndexEOF)
	{
		//this is a stand-in for a zero length event reprsenting EOF
		//the payload is the real sequence number in ascii
		auto payload = ToString(event->GetPayload<EventType::OctetString>(), DataToStringMethod::Raw);
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->debug("{}: OctetString EOF index ({}) event received, payload: '{}'.", Name, index, payload);
		try
		{
			index = std::stoul(payload);
		}
		catch(const std::exception& e)
		{
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: OctetString EOF event with unparsable payload: '{}'.", Name, e.what());
			pIOS->post([=] { (*pStatusCallback)(CommandStatus::NOT_SUPPORTED); });
			return;
		}
		//replace the event with the same thing, but new (sequence number) index and zero-length payload
		EventInfo newevent(event->GetEventType(), index, event->GetSourcePort(), event->GetQuality(), event->GetTimestamp());
		newevent.SetPayload<EventType::OctetString>(OctetStringBuffer());
		event = std::make_shared<const EventInfo>(newevent);
	}

	if(index == pConf->FilenameInfo.Event->GetIndex())
	{
		pTransferTimeoutTimer->expires_from_now(std::chrono::milliseconds(pConf->TransferTimeoutms));
		pTransferTimeoutTimer->async_wait(pSyncStrand->wrap([this,h{handler_tracker}](asio::error_code err){ TransferTimeoutHandler(err); }));

		if(rx_in_progress)
		{
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->debug("{}: Filename buffered: '{}'.", Name, ToString(event->GetPayload<EventType::OctetString>(), DataToStringMethod::Raw).c_str()+crc_size);
			rx_event_buffer[index].push_back(event);
			pIOS->post([=] { (*pStatusCallback)(CommandStatus::SUCCESS); });
			return;
		}

		if(pConf->UseCRCs)
		{
			auto rx_crc = *(uint16_t*)event->GetPayload<EventType::OctetString>().data();
			if(!transfer_reset || pConf->UseConfirms)
			{
				if(rx_crc != crc)
				{
					if(auto log = odc::spdlog_get("FileTransferPort"))
						log->error("{}: Filename CRC mismatch (0x{:04x} != 0x{:04x}). Dropping data '{}'", Name, rx_crc, crc, ToString(event->GetPayload<EventType::OctetString>(), DataToStringMethod::Raw).c_str()+crc_size);
					pIOS->post([=] { (*pStatusCallback)(CommandStatus::UNDEFINED); });
					ProcessQdFilenames(SenderName);
					return;
				}
			}
			else if(rx_crc == crc_ccitt(nullptr,0)) //transfer aborted and the other end reset CRC, means other end restarted
			{
				if(auto log = odc::spdlog_get("FileTransferPort"))
					log->debug("{}: Detected reset seq/CRC.", Name);
				seq = pConf->SequenceIndexStart;
			}
			crc = crc_ccitt((uint8_t*)(event->GetPayload<EventType::OctetString>().data())+crc_size,event->GetPayload<EventType::OctetString>().size()-crc_size,rx_crc);
		}

		//We need to fill out filename template with event and optionally date
		std::string templated_name = pConf->FilenameInfo.Template;
		auto pos = templated_name.find(pConf->FilenameInfo.EventToken);
		if(pos != templated_name.npos)
		{
			templated_name.erase(pos, pConf->FilenameInfo.EventToken.size());
			templated_name.insert(pos, ToString(event->GetPayload<EventType::OctetString>(), DataToStringMethod::Raw).c_str()+crc_size);
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
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->debug("{}: Filename processed: '{}'.", Name, Filename);

		rx_in_progress = true;
		transfer_reset = false;

		auto path = std::filesystem::path(pConf->Directory) / std::filesystem::path(Filename);
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->debug("{}: Start RX, writing '{}'.", Name, path.string());
		if(pConf->Mode == OverwriteMode::FAIL && std::filesystem::exists(path))
		{
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: File '{}' already exists and OverwriteMode::FAIL", Name, path.string());
			pIOS->post([=] { (*pStatusCallback)(CommandStatus::BLOCKED); });
			return;
		}
		std::ios::openmode open_flags = std::ios::binary;
		if(pConf->Mode == OverwriteMode::APPEND)
			open_flags |= std::ios::app;
		fout.open(path,open_flags);
		if(fout.fail())
		{
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: Failed to open file '{}' for writing.", Name, path.string());
			pIOS->post([=] { (*pStatusCallback)(CommandStatus::BLOCKED); });
			return;
		}
		pIOS->post([=] { (*pStatusCallback)(CommandStatus::SUCCESS); });

		//there could already be out-of-order file data
		ProcessRxBuffer(SenderName);
		return;
	}

	if(index >= pConf->SequenceIndexStart && index <= pConf->SequenceIndexStop)
	{
		pTransferTimeoutTimer->expires_from_now(std::chrono::milliseconds(pConf->TransferTimeoutms));
		pTransferTimeoutTimer->async_wait(pSyncStrand->wrap([this,h{handler_tracker}](asio::error_code err){ TransferTimeoutHandler(err); }));

		//push and pop everything through the Q for simplicity - optimise if it pops up in a profile as a hot path
		rx_event_buffer[index].push_back(event);
		//call the callback now because we've successfully queued the event
		pIOS->post([=] { (*pStatusCallback)(CommandStatus::SUCCESS); });

		if(index != seq)
		{
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->trace("{}: Out-of-order sequence number {} buffered {} sequences deep.", Name, index, rx_event_buffer[index].size());
		}

		if(!rx_in_progress)
		{
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->trace("{}: File data (seq={}) before file name/start buffered {} sequences deep.", Name, index, rx_event_buffer[index].size());
			return;
		}

		ProcessRxBuffer(SenderName);
		return;
	}

	//it's not a filename, or file data at this point
	pIOS->post([=] { (*pStatusCallback)(CommandStatus::UNDEFINED); });
	return;
}

//called strand-wrapped from timer
void FileTransferPort::TransferTimeoutHandler(const asio::error_code err)
{
	if(err || !enabled)
		return;

	size_t Qsize = 0;
	for(auto& [idx,q] : rx_event_buffer)
		Qsize += q.size();

	if(rx_in_progress || Qsize > 0)
	{
		auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
		if(pConf->UseConfirms)
		{
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: Transfer timeout. Sending negative confirmation", Name);
			auto confirm_event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock,pConf->ConfirmControlIndex);
			ControlRelayOutputBlock CROB;
			CROB.status = CommandStatus::TIMEOUT;
			CROB.onTimeMS = seq;
			CROB.offTimeMS = pConf->UseCRCs ? crc : 0;
			if(!rx_in_progress)
				CROB.count = 2; //Unsol
			confirm_event->SetPayload<EventType::ControlRelayOutputBlock>(std::move(CROB));
			PublishEvent(confirm_event);
		}
		else
		{
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: Transfer timeout.", Name);
			ResetTransfer();
		}
	}
	rx_event_buffer.clear();
}

//called strand-wrapped from timer, or on-strand from TransferTimeoutHandler()
inline void FileTransferPort::ResetTransfer()
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	pIdleTimer->expires_from_now(std::chrono::milliseconds(pConf->SequenceResetIdleTimems));
	pIdleTimer->async_wait(pSyncStrand->wrap([this,h{handler_tracker}](asio::error_code err){ if(!err) ResetTransfer();}));
	if(transfer_reset)
		return;

	transfer_reset = true;
	if(auto log = odc::spdlog_get("FileTransferPort"))
		log->debug("{}: Resetting seq/CRC.", Name);

	if(pConf->Direction == TransferDirection::RX)
	{
		if(pConf->UseConfirms)
		{
			crc = crc_ccitt(nullptr,0);
			seq = pConf->SequenceIndexStart;
		}
		else
		{
			// seq/CRC will sync with next next filename becaue transfer_reset is true
			// just need to abort any partial transfer
		}
		if(rx_in_progress)
		{
			auto abrt_path = std::filesystem::path(pConf->Directory) / std::filesystem::path(Filename);
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: Aborting RX. Deleting partial file '{}'.", Name, abrt_path.string());
			rx_in_progress = false;
			fout.close();
			std::filesystem::remove(abrt_path);
		}
	}
	else
	{
		crc = crc_ccitt(nullptr,0);
		seq = pConf->SequenceIndexStart;
		//re-Q files that haven't been confirmed
		tx_filename_q.insert(tx_uncofirmed_transfers.begin(),tx_uncofirmed_transfers.end());
		tx_uncofirmed_transfers.clear();
		tx_event_buffer.clear();
		if(tx_in_progress)
		{
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: Aborting TX. Closing file.", Name);
			fin.close(); //this will cause the send chain to bail out
			pConfirmTimer->cancel();
			ConfirmHandler();
			ConfirmHandler = [] {};
		}
	}
}

//called on-strand from ProcessRxBuffer()
inline void FileTransferPort::ConfirmCheck()
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	if(pConf->UseConfirms && seq == pConf->SequenceIndexStop)
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->trace("{}: Sending confirmation full sequence received.", Name);
		auto confirm_event = std::make_shared<EventInfo>(EventType::ControlRelayOutputBlock,pConf->ConfirmControlIndex);
		confirm_event->SetPayload<EventType::ControlRelayOutputBlock>(ControlRelayOutputBlock());
		PublishEvent(confirm_event);
	}
}

//called on-strand
inline void FileTransferPort::BumpSequence()
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	if(++seq > pConf->SequenceIndexStop) seq = pConf->SequenceIndexStart;
	pIdleTimer->expires_from_now(std::chrono::milliseconds(pConf->SequenceResetIdleTimems));
	pIdleTimer->async_wait(pSyncStrand->wrap([this,h{handler_tracker}](asio::error_code err){ if(!err) ResetTransfer();}));
}

//called on-strand from RxEvent()
void FileTransferPort::ProcessQdFilenames(const std::string& SenderName)
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	auto& idx = pConf->FilenameInfo.Event->GetIndex();
	if(!rx_event_buffer[idx].empty())
	{
		auto popped = rx_event_buffer[idx].front();
		rx_event_buffer[idx].pop_front();
		RxEvent(popped,SenderName,std::make_shared<std::function<void (CommandStatus)>>([] (CommandStatus){}));
	}
}

//called on-strand from RxEvent()
void FileTransferPort::ProcessRxBuffer(const std::string& SenderName)
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());

	while(!rx_event_buffer[seq].empty())
	{
		auto popped = rx_event_buffer[seq].front();
		rx_event_buffer[seq].pop_front();
		auto OSBuffer = popped->GetPayload<EventType::OctetString>();

		if(OSBuffer.size() < crc_size)
		{
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: OctetString without CRC received when UseCRCs == true.", Name);
			continue;
		}
		if(pConf->UseCRCs)
		{
			auto rx_crc = *(uint16_t*)OSBuffer.data();
			if(rx_crc != crc)
			{
				if(auto log = odc::spdlog_get("FileTransferPort"))
					log->error("{}: CRC mismatch (0x{:04x} != 0x{:04x}). Dropping data", Name, rx_crc, crc);
				continue;
			}
			crc = crc_ccitt((uint8_t*)(OSBuffer.data())+crc_size,OSBuffer.size()-crc_size,rx_crc);
		}

		if(OSBuffer.size() == (pConf->UseCRCs ? crc_size : 0)) //EOF
		{
			fout.flush();
			fout.close();
			rx_in_progress = false;
			++FilesTransferred;
			ConfirmCheck();
			BumpSequence();
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->debug("{}: Finished writing '{}'.", Name, (std::filesystem::path(pConf->Directory) / std::filesystem::path(Filename)).string());

			//we could already have events from the next file
			ProcessQdFilenames(SenderName);
			return;
		}

		if(fout)
		{
			if(!fout.write(static_cast<const char*>(OSBuffer.data())+crc_size,OSBuffer.size()-crc_size))
			{
				if(auto log = odc::spdlog_get("FileTransferPort"))
					log->error("{}: Mid-RX writing failed on '{}'.", Name, (std::filesystem::path(pConf->Directory) / std::filesystem::path(Filename)).string());
			}
			else
			{
				if(auto log = odc::spdlog_get("FileTransferPort"))
					log->trace("{}: Wrote '{}' to {}.", Name, popped->GetPayloadString(), (std::filesystem::path(pConf->Directory) / std::filesystem::path(Filename)).string());
			}
		}
		//else - we should have already logged an error when fout went bad.

		FileBytesTransferred += OSBuffer.size()-crc_size;
		ConfirmCheck();
		BumpSequence();
	}
}

//called on-strand by TrySend(),SendChunk(), and SendEOF()
void FileTransferPort::TXBufferPublishEvent(std::shared_ptr<EventInfo> event, SharedStatusCallback_t pStatusCallback)
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	PublishEvent(event,pStatusCallback);
	if(pConf->UseConfirms)
		tx_event_buffer.push_back(event);
}

//called on-strand by ConfirmEvent()
void FileTransferPort::ResendFrom(const size_t expected_seq, const uint16_t expected_crc)
{
	// go through and try to find the event with the expected seq and crc
	const auto b = tx_event_buffer.begin();
	bool found = false;
	for(auto ev_it = b; ev_it != tx_event_buffer.end(); ev_it++)
	{
		const auto& bufSeq = (*ev_it)->GetIndex();
		const auto& OSBuf = (*ev_it)->GetPayload<EventType::OctetString>();
		if(bufSeq == expected_seq)
		{
			if(expected_crc && expected_crc != *(uint16_t*)OSBuf.data())
				continue;
			tx_event_buffer.erase(b, ev_it==b ? b : ev_it);
			found = true;
			break;
		}
	}
	if(!found)
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->error("{}: Negative confirmation expected seq/crc ({}/0x{:04x}) that doesn't exist in buffer", Name, expected_seq, expected_crc);
		ResetTransfer();
		return;
	}
	for(const auto& e : tx_event_buffer)
		PublishEvent(e);
}

void FileTransferPort::StartConfirmTimer()
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	pConfirmTimer->expires_from_now(std::chrono::milliseconds(pConf->TransferTimeoutms));
	pConfirmTimer->async_wait(pSyncStrand->wrap([this,h{handler_tracker}](asio::error_code err)
		{
			if(err || !enabled)
				return;
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->error("{}: Confirm timeout.", Name);
			for(const auto& e : tx_event_buffer)
				PublishEvent(e);
			StartConfirmTimer();
		}));
}

//called on-strand by SendChunk()
void FileTransferPort::SendEOF(const std::string path)
{
	std::function<void()> next_action = [this]
							{
								if(!enabled)
									return;
								if(transfer_reset)
								{
									tx_in_progress = false;
									transfer_reset = false;
								}
								if(!tx_filename_q.empty())
								{
									//we could just call TrySend since we're on the strand
									//, but that would be 'cutting in line' ahead of other posted handlers
									pSyncStrand->post([this,h{handler_tracker},next{*tx_filename_q.begin()}]
										{
											auto& [next_path,next_tx_name] = next;
											TrySend(next_path, next_tx_name);
										});
									return;
								}
								auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
								if(pConf->Persistence == ModifiedTimePersistence::ONDISK)
									SaveModTimes();
							};
	if(transfer_reset)
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->debug("{}: TX stream terminated.", Name);
		next_action();
		return;
	}

	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	if(pConf->SequenceIndexEOF >= 0)
	{
		//Send special OctetString as EOF
		auto eof_event = std::make_shared<EventInfo>(EventType::OctetString, pConf->SequenceIndexEOF, Name);
		eof_event->SetPayload<EventType::OctetString>(OctetStringBuffer(std::to_string(seq)));
		TXBufferPublishEvent(eof_event);
	}
	else
	{
		//Send an empty OctetString as EOF
		auto eof_event = std::make_shared<EventInfo>(EventType::OctetString, seq, Name);
		if(pConf->UseCRCs)
		{
			std::string crc_data((char*)&crc,crc_size);
			eof_event->SetPayload<EventType::OctetString>(OctetStringBuffer(std::move(crc_data)));
		}
		else
			eof_event->SetPayload<EventType::OctetString>(OctetStringBuffer());
		TXBufferPublishEvent(eof_event);
	}
	BumpSequence();
	++FilesTransferred;

	//remove this path from the Q, as we've just sent it
	if(pConf->UseConfirms)
		tx_uncofirmed_transfers.insert(tx_filename_q.extract(path));
	else
		tx_filename_q.erase(path);

	if(auto log = odc::spdlog_get("FileTransferPort"))
		log->debug("{}: Finished TX file '{}'.", Name, path);

	tx_in_progress = false;

	if(pConf->UseConfirms && seq == pConf->SequenceIndexStart)
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->debug("{}: Set post-confirmation action: Check TX filename queue.", Name);
		ConfirmHandler = next_action;
		StartConfirmTimer();
	}
	else
		next_action();
}

//called on-strand by SendChunk() or ConfirmHandler()
void FileTransferPort::ScheduleNextChunk(const std::string path, const std::chrono::time_point<std::chrono::high_resolution_clock> start_time, uint64_t bytes_sent)
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	if(pConf->ThrottleBaudrate > 0)
	{
		std::chrono::microseconds throttled_time((bytes_sent*8000000)/pConf->ThrottleBaudrate);
		auto elapsed = std::chrono::high_resolution_clock::now() - start_time;
		auto time_to_wait = elapsed < throttled_time ? throttled_time - elapsed : std::chrono::microseconds::zero();
		pThrottleTimer->expires_from_now(time_to_wait);
		pThrottleTimer->async_wait(pSyncStrand->wrap([this,h{handler_tracker},p{std::move(path)},st{std::move(start_time)},bs{std::move(bytes_sent)}](asio::error_code err)
			{
				if(!err && enabled) SendChunk(std::move(p),std::move(st),std::move(bs));
			}));
	}
	else
	{
		pSyncStrand->post([this,h{handler_tracker},p{std::move(path)},st{std::move(start_time)},bs{std::move(bytes_sent)}]
			{
				if(enabled) SendChunk(std::move(p),std::move(st),std::move(bs));
			});
	}
}

//called as strand wrapped callback after TrySend(), or posted on strand 'recursively'
void FileTransferPort::SendChunk(const std::string path, const std::chrono::time_point<std::chrono::high_resolution_clock> start_time, uint64_t bytes_sent)
{
	if(transfer_reset)
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->debug("{}: Reset short circuit to EOF.", Name);
		fin.close();
		SendEOF(std::move(path));
		return;
	}

	std::function<void()> next_action = [] {};

	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());

	auto file_data_chunk = std::vector<char>(255);
	if(pConf->UseCRCs)
		memcpy(file_data_chunk.data(),&crc,crc_size);

	fin.read(file_data_chunk.data()+crc_size,255-crc_size);
	auto data_size = fin.gcount();
	bool anotherChunkAvailable = false;
	if(data_size > 0)
	{
		if(data_size+crc_size < 255)
			file_data_chunk.resize(data_size+crc_size);
		if(pConf->UseCRCs)
			crc = crc_ccitt((uint8_t*)(file_data_chunk.data())+crc_size,file_data_chunk.size()-crc_size,crc);

		auto chunk_event = std::make_shared<EventInfo>(EventType::OctetString, seq, Name);
		chunk_event->SetPayload<EventType::OctetString>(OctetStringBuffer(std::move(file_data_chunk)));
		TXBufferPublishEvent(chunk_event);
		BumpSequence();
		FileBytesTransferred += data_size;
		bytes_sent += data_size;
		if(fin)
		{
			anotherChunkAvailable = true;
			next_action = [this,h{handler_tracker},p{std::move(path)},st{std::move(start_time)},bs{std::move(bytes_sent)}]
					  {
						  if(enabled) ScheduleNextChunk(std::move(p),std::move(st),std::move(bs));
					  };
		}
	}
	else
	{
		fin.close();
		SendEOF(std::move(path));
		return;
	}

	if(!anotherChunkAvailable)
	{
		fin.close();
		next_action = [this,h{handler_tracker},p{std::move(path)}]
				  {
					  if(enabled) SendEOF(std::move(p));
				  };
	}

	if(pConf->UseConfirms && seq == pConf->SequenceIndexStart)
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->debug("{}: Set post-confirmation action: {}.", Name, anotherChunkAvailable ? "ScheduleNextChunk()" : "SendEOF()");
		ConfirmHandler = next_action;
		StartConfirmTimer();
	}
	else
		next_action();
}

//called on strand by TxPath(), or posted on strand by callback
void FileTransferPort::TrySend(const std::string& path, std::string tx_name)
{
	if(!enabled)
		return;
	if(auto log = odc::spdlog_get("FileTransferPort"))
		log->debug("{}: TrySend(): '{}', buffer size {}.", Name, path, tx_filename_q.size());

	tx_filename_q[path] = tx_name;

	if(tx_in_progress)
		return;

	tx_in_progress = true;
	transfer_reset = false;

	auto send_file = std::make_shared<std::function<void (CommandStatus)>>(pSyncStrand->wrap([this,path,h{handler_tracker}](CommandStatus)
		{
			if(!enabled)
				return;
			if(auto log = odc::spdlog_get("FileTransferPort"))
				log->debug("{}: Start TX file '{}'.", Name, path);

			fin.clear();
			fin.open(path, std::ios::binary);
			if (fin.fail())
			{
			      if(auto log = odc::spdlog_get("FileTransferPort"))
					log->error("{}: Failed to open file path for reading: '{}'",Name,path);
			      tx_in_progress = false;
			      tx_filename_q.erase(path);
			      if(!tx_filename_q.empty())
			      {
			            pSyncStrand->post([this,h{handler_tracker},next{*tx_filename_q.begin()}]
						{
							auto& [next_path,next_tx_name] = next;
							TrySend(next_path, next_tx_name);
						});
				}
			      return;
			}
			SendChunk(path,std::chrono::high_resolution_clock::now());
		}));

	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	auto name_event = std::make_shared<EventInfo>(*pConf->FileNameTransmissionEvent);
	name_event->SetTimestamp();

	if(tx_name == "")
		tx_name = "StartFileTransmission";

	std::vector<char> filename_data(tx_name.size()+crc_size);
	if(pConf->UseCRCs)
		memcpy(filename_data.data(),&crc,crc_size);
	memcpy(filename_data.data()+crc_size,tx_name.data(),tx_name.size());
	name_event->SetPayload<EventType::OctetString>(OctetStringBuffer(std::move(filename_data)));

	if(pConf->UseCRCs)
		crc = crc_ccitt((uint8_t*)(tx_name.data()),tx_name.size(),crc);

	if(auto log = odc::spdlog_get("FileTransferPort"))
		log->debug("{}: TX filename/start event '{}' for '{}'.", Name, tx_name, path);
	TXBufferPublishEvent(name_event,send_file);
}

//called on strand direct from Event_() trigger, or indirectly through Tx()
void FileTransferPort::TxPath(std::string path, const std::string& tx_name, bool only_modified)
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
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->warn("{}: Requested path not in transfer directory: '{}'", Name, std_path.string());
		return;
	}

	path = std_path.string();

	if(!std::filesystem::exists(path))
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->warn("{}: File not found: '{}'", Name, path);
		FileModTimes.erase(path);
		return;
	}

	if(only_modified)
	{
		auto updated_time = std::filesystem::last_write_time(path);

		if((std::filesystem::file_time_type::clock::now() - updated_time) < pConf->ModifiedDwellTimems)
			return;

		auto last_update_it = FileModTimes.find(path);
		if(last_update_it != FileModTimes.end())
		{
			bool has_been_updated = (updated_time > last_update_it->second);

			//store the updated time whether it's newer or not,
			//	to correct any millisecond rounding if parsed from file
			last_update_it->second = updated_time;

			if(!has_been_updated)
				return;
		}
		else //it's new
			FileModTimes[path] = updated_time;
	}

	//if we get to here, the path needs sending
	TrySend(path,tx_name);
}

//called on strand from Event_() trigger or Periodic()
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

//called on strand direct from Event_() trigger, or indirectly through Tx()
std::pair<bool,std::string> FileTransferPort::FileNameTransmissionMatch(const std::string& filename)
{
	auto pConf = static_cast<FileTransferPortConf*>(this->pConf.get());
	std::smatch match_results;
	if(std::regex_match(filename, match_results, *pConf->pFilenameRegex))
	{
		if(match_results.size() > pConf->FileNameTransmissionMatchGroup)
			return {true,match_results.str(pConf->FileNameTransmissionMatchGroup)};
		return {true,filename};
	}
	return {false,""};
}

//called from c'tor
void FileTransferPort::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	auto log = odc::spdlog_get("FileTransferPort");
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
	if(JSONRoot.isMember("ModifiedDwellTimems"))
	{
		pConf->ModifiedDwellTimems = std::chrono::milliseconds(JSONRoot["ModifiedDwellTimems"].asUInt());
	}
	if(JSONRoot.isMember("ModifiedTimePersistence"))
	{
		auto persist_str = JSONRoot["ModifiedTimePersistence"].asString();
		if(persist_str == "ONDISK")
			pConf->Persistence = ModifiedTimePersistence::ONDISK;
		else if(persist_str == "INMEMORY")
			pConf->Persistence = ModifiedTimePersistence::INMEMORY;
		else if(persist_str == "PURGEONDISABLE")
			pConf->Persistence = ModifiedTimePersistence::PURGEONDISABLE;
		else if (log)
			log->error("{}: Invalid ModifiedTimePersistence '{}'. Choose 'ONDISK', 'INMEMORY' (default) or 'PURGEONDISABLE'", Name, persist_str);
	}
	if(JSONRoot.isMember("PersistenceFile"))
	{
		pConf->PersistenceFile = JSONRoot["PersistenceFile"].asString();
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
	if(JSONRoot.isMember("ThrottleBaudrate"))
	{
		pConf->ThrottleBaudrate = JSONRoot["ThrottleBaudrate"].asUInt();
	}
	if(JSONRoot.isMember("UseCRCs"))
	{
		pConf->UseCRCs = JSONRoot["UseCRCs"].asBool();
	}
	if(JSONRoot.isMember("TransferTimeoutms"))
	{
		pConf->TransferTimeoutms = JSONRoot["TransferTimeoutms"].asUInt();
	}
	if(JSONRoot.isMember("SequenceResetIdleTimems"))
	{
		pConf->SequenceResetIdleTimems = JSONRoot["SequenceResetIdleTimems"].asUInt();
	}
	if(JSONRoot.isMember("UseConfirms"))
	{
		pConf->UseConfirms = JSONRoot["UseConfirms"].asBool();
	}
	if(JSONRoot.isMember("ConfirmControlIndex"))
	{
		pConf->ConfirmControlIndex = JSONRoot["ConfirmControlIndex"].asUInt();
	}
}

//called async from anywhere
const Json::Value FileTransferPort::GetStatistics() const
{
	Json::Value ret;
	ret["FilesTransferred"] = Json::UInt(FilesTransferred);
	ret["FileBytesTransferred"] = Json::UInt(FileBytesTransferred);
	ret["TxFileDirCount"] = Json::UInt(TxFileDirCount);
	ret["TxFileMatchCount"] = Json::UInt(TxFileMatchCount);
	ret["IsReset"] = bool(transfer_reset);
	return ret;
}
