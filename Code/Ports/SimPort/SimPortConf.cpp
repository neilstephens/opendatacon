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
#include "Log.h"

SimPortConf::SimPortConf():
	abs_analogs(false),
	std_dev_scaling(1),
	ApplyTimeSyncEvents(true),
	m_name("")
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
	if (json_root.isMember("ApplyTimeSyncEvents"))
		ApplyTimeSyncEvents = json_root["ApplyTimeSyncEvents"].asBool();

	if (json_root.isMember("AbsAnalogs"))
		abs_analogs = json_root["AbsAnalogs"].asBool();
	if (json_root.isMember("StdDevScaling"))
		std_dev_scaling = json_root["StdDevScaling"].asDouble();

	if (json_root.isMember("SQLite3Defaults"))
	{
		auto sqlite = json_root["SQLite3Defaults"];
		if(sqlite.isMember("TimestampHandling"))
			m_db_defaults.timestamp_handling = m_ParseTimestampHandling(sqlite["TimestampHandling"].asString());
		if(sqlite.isMember("File"))
			m_db_defaults.db_filename = sqlite["File"].asString();
		if(sqlite.isMember("Query"))
			m_db_defaults.db_query = sqlite["Query"].asString();
	}

	if(json_root.isMember("Analogs"))
		m_ProcessAnalogs(json_root["Analogs"]);
	if(json_root.isMember("Binaries"))
		m_ProcessBinaries(json_root["Binaries"]);
	if(json_root.isMember("BinaryControls"))
		m_ProcessBinaryControls(json_root["BinaryControls"]);
}

DB_STATEMENT SimPortConf::GetDBStat(const EventType ev_type, const size_t index) const
{
	auto db_stat_it = m_db_stats.find(ToString(ev_type)+std::to_string(index));
	if(db_stat_it != m_db_stats.end())
		return db_stat_it->second;
	return nullptr;
}

TimestampMode SimPortConf::TimestampHandling(const EventType ev_type, const size_t index) const
{
	auto tsh_it = m_db_ts_handling.find(ToString(ev_type)+std::to_string(index));
	if(tsh_it != m_db_ts_handling.end())
		return tsh_it->second;
	return m_db_defaults.timestamp_handling;
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

void SimPortConf::SetLatestControlEvent(std::shared_ptr<odc::EventInfo> event, std::size_t index)
{
	m_pport_data->SetLatestControlEvent(event, index);
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

odc::QualityFlags SimPortConf::StartQuality(odc::EventType type, std::size_t index) const
{
	return m_pport_data->StartQuality(type, index);
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

Json::Value SimPortConf::CurrentState() const
{
	return m_pport_data->CurrentState();
}

std::string SimPortConf::CurrentState(odc::EventType type, std::vector<std::size_t>& indexes) const
{
	return m_pport_data->CurrentState(type, indexes);
}

ptimer_t SimPortConf::Timer(const std::string& name) const
{
	return m_pport_data->Timer(name);
}

void SimPortConf::CancelTimers()
{
	m_pport_data->CancelTimers();
}

std::vector<std::size_t> SimPortConf::Indexes(odc::EventType type) const
{
	return m_pport_data->Indexes(type);
}

bool SimPortConf::IsIndex(odc::EventType type, std::size_t index) const
{
	return m_pport_data->IsIndex(type, index);
}

std::vector<std::shared_ptr<BinaryFeedback>> SimPortConf::BinaryFeedbacks(std::size_t index) const
{
	return m_pport_data->BinaryFeedbacks(index);
}

std::shared_ptr<PositionFeedback> SimPortConf::GetPositionFeedback(std::size_t index) const
{
	return m_pport_data->GetPositionFeedback(index);
}

TimestampMode SimPortConf::m_ParseTimestampHandling(const std::string& ts_mode) const
{
	TimestampMode tsm = m_db_defaults.timestamp_handling;
	if(ts_mode == "ABSOLUTE")
		tsm = TimestampMode::ABSOLUTE_T;
	else if(ts_mode == "ABSOLUTE_FASTFORWARD")
		tsm = TimestampMode::ABSOLUTE_T | TimestampMode::FASTFORWARD;
	else if(ts_mode == "RELATIVE_TOD")
		tsm = TimestampMode::TOD;
	else if(ts_mode == "RELATIVE_TOD_FASTFORWARD")
		tsm = TimestampMode::TOD | TimestampMode::FASTFORWARD;
	else if(ts_mode == "RELATIVE_FIRST")
		tsm = TimestampMode::FIRST;
	else Log.Error("Defaulting invalid SQLite3 'TimeStampHandling' mode '{}'.", ts_mode);
	return tsm;
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
		Log.Error("A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '{}'", data.toStyledString());
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
			odc::QualityFlags flag = odc::QualityFlags::ONLINE;
			if (analogs[i].isMember("SQLite3"))
				m_ProcessSQLite3(analogs[i]["SQLite3"], "Analog", index);
			if (analogs[i].isMember("StdDev"))
				std_dev = analogs[i]["StdDev"].asDouble();
			if (analogs[i].isMember("UpdateIntervalms"))
				update_interval = analogs[i]["UpdateIntervalms"].asUInt();
			if (analogs[i].isMember("StartVal"))
			{
				std::string str_start_val = to_lower(analogs[i]["StartVal"].asString());
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
			}
			else
				flag = (odc::QualityFlags::COMM_LOST | odc::QualityFlags::RESTART);

			m_pport_data->CreateEvent(odc::EventType::Analog, index, m_name, flag, std_dev, update_interval, start_val);
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
			Log.Error("A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '{}'", binaries[n].toStyledString());
			continue;
		}
		for(auto index = start; index <= stop; index++)
		{
			bool val = false;
			double std_dev = 0.0f;
			std::size_t update_interval = 0;
			odc::QualityFlags flag = odc::QualityFlags::ONLINE;
			if (binaries[n].isMember("SQLite3"))
				m_ProcessSQLite3(binaries[n]["SQLite3"], "Binary", index);
			if (binaries[n].isMember("UpdateIntervalms"))
				update_interval = binaries[n]["UpdateIntervalms"].asUInt();
			if (binaries[n].isMember("StartVal"))
			{
				if (binaries[n]["StartVal"].asString() == "X")
					flag = odc::QualityFlags::COMM_LOST;
				else
					val = binaries[n]["StartVal"].asBool();
			}
			else
				flag = (odc::QualityFlags::COMM_LOST | odc::QualityFlags::RESTART);
			m_pport_data->CreateEvent(odc::EventType::Binary, index, m_name, flag, std_dev, update_interval, val);
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
			Log.Error("A point needs an \"Index\" or a \"Range\" with a \"Start\" and a \"Stop\" : '{}'", binary_controls[n].toStyledString());
			continue;
		}
		for (auto index = start; index <= stop; index++)
		{
			std::size_t update_interval = 0;
			if (binary_controls[n].isMember("Intervalms"))
				update_interval = binary_controls[n]["Intervalms"].asUInt();
			if (binary_controls[n].isMember("FeedbackBinaries"))
				m_ProcessFeedbackBinaries(binary_controls[n]["FeedbackBinaries"], index, update_interval);
			else if (binary_controls[n].isMember("FeedbackPosition"))
				m_ProcessFeedbackPosition(binary_controls[n]["FeedbackPosition"], index);
			else
				m_pport_data->CreateBinaryControl(index);
		}
	}
}

void SimPortConf::m_ProcessSQLite3(const Json::Value& sqlite, const std::string& type, const std::size_t index)
{
	auto filename = m_db_defaults.db_filename;
	auto query = m_db_defaults.db_query;

	if(sqlite.isMember("File"))
		filename = sqlite["File"].asString();
	if(sqlite.isMember("Query"))
		query = sqlite["Query"].asString();

	sqlite3* db;
	auto rv = sqlite3_open_v2(filename.c_str(),&db,SQLITE_OPEN_READONLY|SQLITE_OPEN_NOMUTEX|SQLITE_OPEN_EXRESCODE,nullptr);
	if(rv != SQLITE_OK)
	{
		Log.Error("Failed to open sqlite3 DB '{}' : '{}'", filename, sqlite3_errstr(rv));
	}
	else
	{
		Log.Debug("Opened sqlite3 DB '{}' for {} {}.", filename, type, index);
		sqlite3_stmt* stmt;
		auto rv = sqlite3_prepare_v2(db,query.c_str(),-1,&stmt,nullptr);
		if(rv != SQLITE_OK)
		{
			Log.Error("Failed to prepare SQLite3 query '{}' for {} {} : '{}'", query, type, index, sqlite3_errstr(rv));
		}
		else if (sqlite3_column_count(stmt) != 2)
		{
			Log.Error("SQLite3Query doesn't return 2 columns (for use as timestamp and value) '{}'", query);
		}
		else
		{
			Log.Debug("Prepared SQLite3 query '{}' for {} {}", query, type, index);
			auto deleter = [](sqlite3_stmt* st){sqlite3_finalize(st);};
			m_db_stats[type+std::to_string(index)] = DB_STATEMENT(stmt,deleter);

			auto index_bind_index = sqlite3_bind_parameter_index(stmt, ":INDEX");
			if(index_bind_index)
			{
				auto rv = sqlite3_bind_int(stmt, index_bind_index, index);
				if(rv != SQLITE_OK)
				{
					Log.Error("Failed to bind index ({}) to prepared SQLite3 query '{}' : '{}'", index, query, sqlite3_errstr(rv));
					m_db_stats.erase(type+std::to_string(index));
				}
				else
				{
					Log.Debug("Bound index ({}) to prepared SQLite3 query '{}'", index, query, sqlite3_errstr(rv));
				}
			}

			auto type_bind_index = sqlite3_bind_parameter_index(stmt, ":TYPE");
			if(type_bind_index)
			{
				auto rv = sqlite3_bind_text(stmt, type_bind_index, type.c_str(), -1, SQLITE_TRANSIENT);
				if(rv != SQLITE_OK)
				{
					Log.Error("Failed to bind type ({}) to prepared SQLite3 query '{}' : '{}'", type, query, sqlite3_errstr(rv));
					m_db_stats.erase(type+std::to_string(index));
				}
				else
				{
					Log.Debug("Bound type ({}) to prepared SQLite3 query '{}'", type, query, sqlite3_errstr(rv));
				}
			}

			if(sqlite.isMember("TimestampHandling"))
				m_db_ts_handling[type+std::to_string(index)] = m_ParseTimestampHandling(sqlite["TimestampHandling"].asString());
		}
	}
}

void SimPortConf::m_ProcessFeedbackBinaries(const Json::Value& feedback_binaries, std::size_t index,
	std::size_t update_interval)
{
	for (Json::ArrayIndex fbn = 0; fbn < feedback_binaries.size(); ++fbn)
	{
		if (!feedback_binaries[fbn].isMember("Index"))
		{
			Log.Error("An 'Index' is required for Binary feedback : '{}'", feedback_binaries[fbn].toStyledString());
			continue;
		}
		auto fb_index = feedback_binaries[fbn]["Index"].asUInt();

		if(!IsIndex(EventType::Binary,fb_index))
		{
			Log.Error("Invalid 'Index' for Binary feedback (it must be a configured binary index) : '{}'", feedback_binaries[fbn].toStyledString());
			continue;
		}

		auto on_qual = QualityFlags::ONLINE;
		auto off_qual = QualityFlags::ONLINE;
		bool on_val = true;
		bool off_val = false;
		auto mode = FeedbackMode::LATCH;
		uint32_t delay = 0;

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
				Log.Warn("Unrecognised feedback mode: '{}'", feedback_binaries[fbn].toStyledString());
			}
		}
		if (feedback_binaries[fbn].isMember("Delayms"))
		{
			delay = feedback_binaries[fbn]["Delayms"].asUInt();
		}

		auto on = std::make_shared<EventInfo>(EventType::Binary, fb_index, m_name, on_qual);
		on->SetPayload<EventType::Binary>(std::move(on_val));
		auto off = std::make_shared<EventInfo>(EventType::Binary, fb_index, m_name, off_qual);
		off->SetPayload<EventType::Binary>(std::move(off_val));
		m_pport_data->CreateBinaryControl(index, on, off, mode, delay, update_interval);
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
	FeedbackType type = FeedbackType::UNDEFINED;
	std::vector<PositionAction> actions(2, PositionAction::UNDEFINED);
	std::vector<std::size_t> indexes;

	//FIXME: should allow negative limits
	std::size_t lower_limit = 0, raise_limit = std::numeric_limits<size_t>::max();
	double tap_step = 1;

	if (feedback_position.isMember("Type"))
		type = ToFeedbackType(feedback_position["Type"].asString());
	if (feedback_position.isMember("Index"))
		indexes.emplace_back(feedback_position["Index"].asUInt());
	if (feedback_position.isMember("Indexes"))
		for (Json::ArrayIndex i = 0; i < feedback_position["Indexes"].size(); ++i)
			indexes.emplace_back(feedback_position["Indexes"][i].asUInt());
	if (feedback_position.isMember("OffAction"))
		actions[OFF] = ToPositionAction(feedback_position["OffAction"].asString());
	if (feedback_position.isMember("OnAction"))
		actions[ON] = ToPositionAction(feedback_position["OnAction"].asString());
	if (feedback_position.isMember("LowerLimit"))
		lower_limit = feedback_position["LowerLimit"].asUInt();
	if (feedback_position.isMember("RaiseLimit"))
		raise_limit = feedback_position["RaiseLimit"].asUInt();
	if (feedback_position.isMember("Step"))
		tap_step = feedback_position["Step"].asDouble();

	//warn if there's not at least one action
	if(actions[ON] == PositionAction::UNDEFINED && actions[OFF] == PositionAction::UNDEFINED)
		Log.Warn("No valid actions defined for Postion feedback : '{}'", feedback_position.toStyledString());

	EventType fb_type;
	switch(type)
	{
		case FeedbackType::BCD:
		case FeedbackType::BINARY:
			fb_type = EventType::Binary;
			break;
		case FeedbackType::ANALOG:
			fb_type = EventType::Analog;
			break;
		default:
			Log.Error("Invalid 'Type' for Postion feedback : '{}'", feedback_position.toStyledString());
			return;
	}

	for(const auto& fb_index : indexes)
		if(!IsIndex(fb_type,fb_index))
		{
			Log.Error("Invalid 'Index(es)' for Postion feedback (they must be configured points) : '{}'", feedback_position.toStyledString());
			return;
		}

	m_pport_data->CreateBinaryControl(index, m_name, type, indexes, actions, lower_limit, raise_limit, tap_step);
}
