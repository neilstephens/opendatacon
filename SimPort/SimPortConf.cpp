/*	opendatacon
 *
 *	Copyright (c) 2020:
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
 * SimPortConf.cpp
 *
 *  Created on: 15/07/2020
 *  The year of bush fires and pandemic
 *      Author: Rakesh Kumar <cpp.rakesh@gmail.com>
 */

#include "SimPortConf.h"

SimPortConf::SimPortConf():
	m_name(""),
	m_timestamp_handling(TimestampMode::FIRST)
{
	m_pport_data = std::make_shared<SimPortData>();
}

void SimPortConf::ProcessElements(const Json::Value& json_root)
{
	if (json_root.isMember("HttpIP"))
		m_pport_data->HttpAddress(json_root["HttpIP"].asString());
	if (json_root.isMember("HttpPort"))
		m_pport_data->HttpPort(json_root["HttpPort"].asString());
	if (json_root.isMember("Version"))
		m_pport_data->Version(json_root["Version"].asString());

	if(json_root.isMember("Analogs"))
		m_ProcessAnalogs(json_root["Analogs"]);
	if(json_root.isMember("Binaries"))
		m_ProcessBinaries(json_root["Binaries"]);
	if(json_root.isMember("BinaryControls"))
		m_ProcessBinaryControls(json_root["BinaryControls"]);
}

std::unordered_map<std::string, DB_STATEMENT> SimPortConf::GetDBStats() const
{
	return m_db_stats;
}

TimestampMode SimPortConf::TimestampHandling() const
{
	return m_timestamp_handling;
}

void SimPortConf::Name(const std::string& name)
{
	m_name = name;
}

double SimPortConf::DefaultStdDev() const
{
	return m_pport_data->DefaultStdDev();
}

std::string SimPortConf::HttpAddress() const
{
	return m_pport_data->HttpAddress();
}

std::string SimPortConf::HttpPort() const
{
	return m_pport_data->HttpPort();
}

std::string SimPortConf::Version() const
{
	return m_pport_data->Version();
}

double SimPortConf::StdDev(std::size_t index) const
{
	return m_pport_data->StdDev(index);
}

void SimPortConf::Event(std::shared_ptr<odc::EventInfo> event)
{
	m_pport_data->Event(event);
}

std::shared_ptr<odc::EventInfo> SimPortConf::Event(odc::EventType type, std::size_t index) const
{
	return m_pport_data->Event(type, index);
}

void SimPortConf::Payload(odc::EventType type, std::size_t index, double payload)
{
	m_pport_data->Payload(type, index, payload);
}

double SimPortConf::Payload(odc::EventType type, std::size_t index) const
{
	return m_pport_data->Payload(type, index);
}

double SimPortConf::StartValue(odc::EventType type, std::size_t index) const
{
	return m_pport_data->StartValue(type, index);
}

void SimPortConf::ForcedState(odc::EventType type, std::size_t index, bool value)
{
	m_pport_data->ForcedState(type, index, value);
}

bool SimPortConf::ForcedState(odc::EventType type, std::size_t index) const
{
	return m_pport_data->ForcedState(type, index);
}

void SimPortConf::UpdateInterval(odc::EventType type, std::size_t index, std::size_t value)
{
	m_pport_data->UpdateInterval(type, index, value);
}

std::size_t SimPortConf::UpdateInterval(odc::EventType type, std::size_t index) const
{
	return m_pport_data->UpdateInterval(type, index);
}

std::vector<std::size_t> SimPortConf::Indexes(odc::EventType type) const
{
	return m_pport_data->Indexes(type);
}

bool SimPortConf::IsIndex(odc::EventType type, std::size_t index) const
{
	return m_pport_data->IsIndex(type, index);
}

Json::Value SimPortConf::CurrentState() const
{
	return m_pport_data->CurrentState();
}

std::string SimPortConf::CurrentState(odc::EventType type, std::vector<std::size_t>& indexes) const
{
	return m_pport_data->CurrentState(type, indexes);
}

std::vector<std::shared_ptr<BinaryFeedback>> SimPortConf::BinaryFeedbacks(std::size_t index) const
{
	return m_pport_data->BinaryFeedbacks(index);
}

std::shared_ptr<BinaryPosition> SimPortConf::GetBinaryPosition(std::size_t index) const
{
	return m_pport_data->GetBinaryPosition(index);
}

bool SimPortConf::m_ParseIndexes(const Json::Value& data, std::size_t& start, std::size_t& stop) const
{
	bool result = true;
	if (data.isMember("Index"))
		start = stop = data["Index"].asUInt();
	else if (data["Range"].isMember("Start") &&
	         data["Range"].isMember("Stop"))
	{
		start = data["Range"]["Start"].asUInt();
		stop = data["Range"]["Stop"].asUInt();
	}
	else
	{
		if(auto log = odc::spdlog_get("SimPort"))
			log->error("A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '{}'", data.toStyledString());
		result = false;
	}
	return result;
}

void SimPortConf::m_ProcessAnalogs(const Json::Value& analogs)
{
	for (Json::ArrayIndex i = 0; i < analogs.size(); ++i)
	{
		std::size_t start = 0;
		std::size_t stop = 0;
		if (!m_ParseIndexes(analogs[i], start, stop))
			continue;

		for (std::size_t index = start; index <= stop; ++index)
		{
			double start_val = 0.0f;
			double std_dev = 0.0f;
			std::size_t update_interval = 0;
			if (analogs[i].isMember("SQLite3"))
				m_ProcessSQLite3(analogs[i]["SQLite3"], index);
			if (analogs[i].isMember("StdDev"))
				std_dev = analogs[i]["StdDev"].asDouble();
			if (analogs[i].isMember("UpdateIntervalms"))
				update_interval = analogs[i]["UpdateIntervalms"].asUInt();
			if (analogs[i].isMember("StartVal"))
			{
				std::string str_start_val = to_lower(analogs[i]["StartVal"].asString());
				odc::QualityFlags flag = odc::QualityFlags::ONLINE;
				if (str_start_val == "nan")
					start_val = std::numeric_limits<double>::quiet_NaN();
				else if (str_start_val == "inf")
					start_val = std::numeric_limits<double>::infinity();
				else if (str_start_val == "-inf")
					start_val = std::numeric_limits<double>::infinity();
				else if (str_start_val == "x")
					flag = odc::QualityFlags::COMM_LOST;
				else
					start_val = std::stod(str_start_val);
				m_pport_data->CreateEvent(odc::EventType::Analog, index, m_name, flag, std_dev, update_interval, start_val);
			}
		}
	}
}

void SimPortConf::m_ProcessBinaries(const Json::Value& binaries)
{
	for(Json::ArrayIndex n = 0; n < binaries.size(); ++n)
	{
		size_t start = 0, stop = 0;
		if(binaries[n].isMember("Index"))
			start = stop = binaries[n]["Index"].asUInt();
		else if(binaries[n]["Range"].isMember("Start") && binaries[n]["Range"].isMember("Stop"))
		{
			start = binaries[n]["Range"]["Start"].asUInt();
			stop = binaries[n]["Range"]["Stop"].asUInt();
		}
		else
		{
			if(auto log = odc::spdlog_get("SimPort"))
				log->error("A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '{}'", binaries[n].toStyledString());
			continue;
		}
		for(auto index = start; index <= stop; index++)
		{
			bool val = false;
			odc::QualityFlags flag = odc::QualityFlags::ONLINE;
			if (binaries[n].isMember("StartVal"))
			{
				if (binaries[n]["StartVal"].asString() == "X")
					flag = odc::QualityFlags::COMM_LOST;
				else
					val = binaries[n]["StartVal"].asBool();
			}
			m_pport_data->CreateEvent(odc::EventType::Binary, index, m_name, flag, 0.0f, 0.0f, val);
		}
	}
}

void SimPortConf::m_ProcessBinaryControls(const Json::Value& binary_controls)
{
	for(Json::ArrayIndex n = 0; n < binary_controls.size(); ++n)
	{
		size_t start, stop;
		if(binary_controls[n].isMember("Index"))
			start = stop = binary_controls[n]["Index"].asUInt();
		else if(binary_controls[n]["Range"].isMember("Start") && binary_controls[n]["Range"].isMember("Stop"))
		{
			start = binary_controls[n]["Range"]["Start"].asUInt();
			stop = binary_controls[n]["Range"]["Stop"].asUInt();
		}
		else
		{
			if(auto log = odc::spdlog_get("SimPort"))
				log->error("A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '{}'", binary_controls[n].toStyledString());
			continue;
		}
		for (auto index = start; index <= stop; index++)
		{
			std::size_t update_interval = 0;
			if (binary_controls[n].isMember("Intervalms"))
				update_interval = binary_controls[n]["Intervalms"].asUInt();
			if (binary_controls[n].isMember("FeedbackBinaries"))
				m_ProcessFeedbackBinaries(binary_controls[n]["FeedbackBinaries"], index, update_interval);
			if (binary_controls[n].isMember("FeedbackPosition"))
				m_ProcessFeedbackPosition(binary_controls[n]["FeedbackPosition"], index);
		}
	}
}

void SimPortConf::m_ProcessSQLite3(const Json::Value& sqlite, std::size_t index)
{
	if(sqlite.isMember("File") && sqlite.isMember("Query"))
	{
		sqlite3* db;
		auto filename = sqlite["File"].asString();
		auto rv = sqlite3_open_v2(filename.c_str(),&db,SQLITE_OPEN_READONLY|SQLITE_OPEN_NOMUTEX|SQLITE_OPEN_SHAREDCACHE,nullptr);
		if(rv != SQLITE_OK)
		{
			if(auto log = odc::spdlog_get("SimPort"))
				log->error("Failed to open sqlite3 DB '{}' : '{}'", filename, sqlite3_errstr(rv));
		}
		else
		{
			sqlite3_stmt* stmt;
			auto query = sqlite["Query"].asString();
			auto rv = sqlite3_prepare_v2(db,query.c_str(),-1,&stmt,nullptr);
			if(rv != SQLITE_OK)
			{
				if(auto log = odc::spdlog_get("SimPort"))
					log->error("Failed to prepare SQLite3 query '{}' : '{}'", query, sqlite3_errstr(rv));
			}
			else if (sqlite3_column_count(stmt) != 2)
			{
				if(auto log = odc::spdlog_get("SimPort"))
					log->error("SQLite3Query doesn't return 2 columns (for use as timestamp and value) '{}'", query);
			}
			else
			{
				auto deleter = [](sqlite3_stmt* st){sqlite3_finalize(st);};
				m_db_stats["Analog"+std::to_string(index)] = DB_STATEMENT(stmt,deleter);
				auto index_bind_index = sqlite3_bind_parameter_index(stmt, ":INDEX");
				if(index_bind_index)
				{
					auto rv = sqlite3_bind_int(stmt, index_bind_index, index);
					if(rv != SQLITE_OK)
					{
						if(auto log = odc::spdlog_get("SimPort"))
							log->error("Failed to bind index ({}) to prepared SQLite3 query '{}' : '{}'", query, sqlite3_errstr(rv));
						m_db_stats.erase("Analog"+std::to_string(index));
					}
				}
			}
			m_timestamp_handling = TimestampMode::FIRST;
			if(sqlite.isMember("TimestampHandling"))
			{
				auto ts_mode =sqlite["TimestampHandling"].asString();
				if(ts_mode == "ABSOLUTE")
					m_timestamp_handling = TimestampMode::ABSOLUTE_T;
				else if(ts_mode == "ABSOLUTE_FASTFORWARD")
					m_timestamp_handling = TimestampMode::ABSOLUTE_T | TimestampMode::FASTFORWARD;
				else if(ts_mode == "RELATIVE_TOD")
					m_timestamp_handling = TimestampMode::TOD;
				else if(ts_mode == "RELATIVE_TOD_FASTFORWARD")
					m_timestamp_handling = TimestampMode::TOD | TimestampMode::FASTFORWARD;
				else if(ts_mode == "RELATIVE_FIRST")
					m_timestamp_handling = TimestampMode::FIRST;
				else if(auto log = odc::spdlog_get("SimPort"))
					log->error("Invalid SQLite3 'TimeStampHandling' mode '{}'. Defaulting to RELATIVE_FIRST", ts_mode);
			}
			else if(auto log = odc::spdlog_get("SimPort"))
				log->info("SQLite3 'TimeStampHandling' mode defaulting to RELATIVE_FIRST");
		}
	}
	else
	{
		if(auto log = odc::spdlog_get("SimPort"))
			log->error("'SQLite3' object requires 'File' and 'Query' for point : '{}'", sqlite.toStyledString());
	}
}

void SimPortConf::m_ProcessFeedbackBinaries(const Json::Value& feedback_binaries, std::size_t index,
	std::size_t update_interval)
{
	for (Json::ArrayIndex fbn = 0; fbn < feedback_binaries.size(); ++fbn)
	{
		if (!feedback_binaries[fbn].isMember("Index"))
		{
			if (auto log = odc::spdlog_get("SimPort"))
				log->error("An 'Index' is required for Binary feedback : '{}'", feedback_binaries[fbn].toStyledString());
			continue;
		}

		auto fb_index = feedback_binaries[fbn]["Index"].asUInt();
		auto on_qual = QualityFlags::ONLINE;
		auto off_qual = QualityFlags::ONLINE;
		bool on_val = true;
		bool off_val = false;
		auto mode = FeedbackMode::LATCH;

		if (feedback_binaries[fbn].isMember("OnValue"))
		{
			if (feedback_binaries[fbn]["OnValue"].asString() == "X")
				on_qual = QualityFlags::COMM_LOST;
			else
				on_val = feedback_binaries[fbn]["OnValue"].asBool();
		}
		if (feedback_binaries[fbn].isMember("OffValue"))
		{
			if (feedback_binaries[fbn]["OffValue"].asString() == "X")
				off_qual = QualityFlags::COMM_LOST;
			else
				off_val = feedback_binaries[fbn]["OffValue"].asBool();

		}
		if (feedback_binaries[fbn].isMember("FeedbackMode"))
		{
			auto mode_str = feedback_binaries[fbn]["FeedbackMode"].asString();
			if (mode_str == "PULSE")
				mode = FeedbackMode::PULSE;
			else if (mode_str == "LATCH")
				mode = FeedbackMode::LATCH;
			else
			{
				if (auto log = odc::spdlog_get("SimPort"))
					log->warn("Unrecognised feedback mode: '{}'", feedback_binaries[fbn].toStyledString());
			}
		}

		auto on = std::make_shared<EventInfo>(EventType::Binary, fb_index, m_name, on_qual);
		on->SetPayload<EventType::Binary>(std::move(on_val));
		auto off = std::make_shared<EventInfo>(EventType::Binary, fb_index, m_name, off_qual);
		off->SetPayload<EventType::Binary>(std::move(off_val));
		m_pport_data->CreateBinaryFeedback(index, on, off, mode, update_interval);
	}
}

/*
  Used to implement TapChanger functionality, containing:
  {"Type": "Analog", "Index" : 7, "FeedbackMode":"PULSE", "Action":"UP", "Limit":10}
  {"Type": "Binary", "Indexes" : "10,11,12", "FeedbackMode":"PULSE", "Action":"UP", "Limit":10}
  {"Type": "BCD", "Indexes" : "10,11,12", "FeedbackMode":"PULSE", "Action":"UP", "Limit":10}

  Because the feedback is complex, we need to keep the current value available so we can change it.
  As a result, the feedback bits or analog will be set to forced state. If you unforce them, they will
  get re-forced the next time a command comes through.
  Bit of a hack, but comes back to the original simulator design not "remembering" what its current state is.
*/
void SimPortConf::m_ProcessFeedbackPosition(const Json::Value& feedback_position, std::size_t index)
{
	odc::FeedbackType type = odc::FeedbackType::UNDEFINED;
	odc::PositionAction action = odc::PositionAction::UNDEFINED;
	std::vector<std::size_t> indexes;
	std::size_t limit = 0;
	if (feedback_position.isMember("Type"))
		type = ToFeedbackType(feedback_position["Type"].asString());
	if (feedback_position.isMember("Index"))
		indexes.emplace_back(feedback_position["Index"].asUInt());
	if (feedback_position.isMember("Indexes"))
		for (Json::ArrayIndex i = 0; i < feedback_position["Indexes"].size(); ++i)
			indexes.emplace_back(feedback_position["Indexes"][i].asUInt());
	if (feedback_position.isMember("Action"))
		action = ToPositionAction(feedback_position["Action"].asString());
	if (feedback_position.isMember("Limit"))
		limit = feedback_position["Limit"].asUInt();
	m_pport_data->CreateBinaryPosition(index, type, indexes, action, limit);
}
