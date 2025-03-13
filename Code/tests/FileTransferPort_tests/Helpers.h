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
 * Helpers.h
 *
 *  Created on: 07/02/2023
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef TESTHELPERS_H
#define TESTHELPERS_H

#include <json/json.h>
#include <opendatacon/IOHandler.h>
#include <opendatacon/DataPort.h>
#include <opendatacon/util.h>
#include <opendatacon/Platform.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fstream>
#include <filesystem>
#include <random>

extern spdlog::level::level_enum log_level;

inline void TestSetup()
{
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto pLibLogger = std::make_shared<spdlog::logger>("FileTransferPort", console_sink);
	pLibLogger->set_level(log_level);
	odc::spdlog_register_logger(pLibLogger);

	// We need an opendatacon logger to catch config file parsing errors
	auto pODCLogger = std::make_shared<spdlog::logger>("opendatacon", console_sink);
	pODCLogger->set_level(log_level);
	odc::spdlog_register_logger(pODCLogger);

	static std::once_flag once_flag;
	std::call_once(once_flag,[]()
		{
			InitLibaryLoading();
		});
}
inline void TestTearDown()
{
	odc::spdlog_drop_all(); // Close off everything
}

inline Json::Value GetConfigJSON(bool TX)
{
	static const char* tx_conf = R"001(
	{
		"Direction" : "TX",
		"Directory" : "./",
		"FilenameRegex" : "(FileTxTest.*\\.bin)",
		"Recursive" : true,
		"FileNameTransmission" : {"Index" : 0, "MatchGroup" : 1},	//send filename and get confirmation before starting transfer
		"SequenceIndexRange" : {"Start" : 1, "Stop" : 15},
		"TransferTriggers" :
		[
			{"Type" : "Periodic", "Periodms" : 100, "OnlyWhenModified" : true},
			{"Type" : "BinaryControl", "Index" : 0, "OnlyWhenModified" : false},
			{"Type" : "AnalogControl", "Index" : 0, "Value" : 3, "OnlyWhenModified" : false},
			{"Type" : "OctetStringPath", "Index" : 0, "OnlyWhenModified" : false}
		],
		"ModifiedDwellTimems" : 500,
		"UseCRCs" : false,
		"UseConfirms" : false
	})001";
	static const char* rx_conf = R"001(
	{
		"Direction" : "RX",
		"Directory" : "./RX",
		"Filename" :
		{
			"InitialName" : "StartingOrFixedFilename.txt",
			"Template" : "<DATE>_<DYNAMIC>",
			"Date" : {"Format" : "dateformat", "Token" : "<DATE>"},
			"Event" : {"Index" : 0, "Token" : "<DYNAMIC>"}
		},
		"OverwriteMode" : "OVERWRITE", 	//or "FAIL" or "APPEND",
		"SequenceIndexRange" : {"Start" : 1, "Stop" : 15, "EOF" : 16},
		"UseCRCs" : false,
		"UseConfirms" : false
	})001";

	std::istringstream iss(TX ? tx_conf : rx_conf);
	Json::CharReaderBuilder JSONReader;
	std::string err_str;
	Json::Value json_conf;
	bool parse_success = Json::parseFromStream(JSONReader,iss, &json_conf, &err_str);
	if (!parse_success)
	{
		if(auto log = odc::spdlog_get("FileTransferPort"))
			log->error("Failed to parse configuration: '{}'", err_str);
	}
	return json_conf;
}

inline bool WriteFile(const std::string& name, size_t size)
{
	std::ofstream fout(name, std::ios::binary);
	if(fout.fail())
		return false;
	size_t i = 0;
	while(i++ < size && fout)
		fout<<uint8_t(rand()%255);
	if(fout.fail())
		return false;
	fout.flush();
	fout.close();
	return true;
}

inline void MakeFiles()
{
	//create some files that match our transfer pattern
	std::filesystem::create_directories(std::filesystem::path("sub/sub"));
	WriteFile("FileTxTest1.bin",1300); //1300 not a multiple of 255
	WriteFile("FileTxTest2.bin",5100); //5100 is a multiple of 255
	WriteFile("sub/FileTxTest3.bin",1300);
	WriteFile("sub/FileTxTest4.bin",5100);
	WriteFile("sub/sub/FileTxTest5.bin",1300);
	WriteFile("sub/sub/FileTxTest6.bin",5100);
	//and a few that don't match (just in case there aren't any)
	WriteFile("FileTxTest7.notbin",13);
	WriteFile("FileTxTest8.notbin",51);
	WriteFile("sub/FileTxTest9.notbin",13);
	WriteFile("sub/FileTxTest10.notbin",51);
	WriteFile("sub/sub/FileTxTest11.notbin",13);
	WriteFile("sub/sub/FileTxTest12.notbin",51);
	std::filesystem::create_directories(std::filesystem::path("RX"));
}

inline void ClearFiles()
{
	//clear the slate
	std::remove("FileTxTest1.bin");
	std::remove("FileTxTest2.bin");
	std::remove("sub/FileTxTest3.bin");
	std::remove("sub/FileTxTest4.bin");
	std::remove("sub/sub/FileTxTest5.bin");
	std::remove("sub/sub/FileTxTest6.bin");
	std::remove("FileTxTest7.notbin");
	std::remove("FileTxTest8.notbin");
	std::remove("sub/FileTxTest9.notbin");
	std::remove("sub/FileTxTest10.notbin");
	std::remove("sub/sub/FileTxTest11.notbin");
	std::remove("sub/sub/FileTxTest12.notbin");
	std::remove("RX/dateformat_FileTxTest1.bin");
	std::remove("RX/dateformat_FileTxTest2.bin");
	std::remove("RX/dateformat_FileTxTest3.bin");
	std::remove("RX/dateformat_FileTxTest4.bin");
	std::remove("RX/dateformat_FileTxTest5.bin");
	std::remove("RX/dateformat_FileTxTest6.bin");
}

class CorruptConnector: public odc::IOHandler
{
public:
	void Enable() override {}
	void Disable() override {}

	CorruptConnector(std::shared_ptr<odc::DataPort> P1, std::shared_ptr<odc::DataPort> P2):
		IOHandler("Nasty"),
		Port1(P1),
		Port2(P2)
	{
		Port1->Subscribe(this,Name);
		Port2->Subscribe(this,Name);
	}

	void Event(std::shared_ptr<const odc::EventInfo> event, const std::string& SenderName, odc::SharedStatusCallback_t pStatusCallback) override
	{
		auto Sendee = Port1;
		if(SenderName == Port1->GetName())
			Sendee = Port2;

		if(event->GetEventType() == odc::EventType::OctetString)
		{
			std::random_device rd;
			std::mt19937 rand_num_gen(rd());
			std::uniform_int_distribution<> even_chances(1,10);
			switch(even_chances(rand_num_gen))
			{
				//drop data
				case 1:
					if(auto log = odc::spdlog_get("opendatacon"))
						log->trace("DROPPED: {} {} Payload {} Event {} => DROP", ToString(event->GetEventType()),event->GetIndex(), event->GetPayloadString(), Name);
					(*pStatusCallback)(odc::CommandStatus::SUCCESS);
					return;

				//duplicate data
				case 2: case 3: case 4: case 5: case 6:
					PublishEvent(event,Sendee,pStatusCallback);
					PublishEvent(event,Sendee,std::make_shared<std::function<void (odc::CommandStatus)>>([] (odc::CommandStatus){}));
					return;

				//be nice
				default:
					PublishEvent(event,Sendee,pStatusCallback);
					return;
			}
		}
		else
			PublishEvent(event,Sendee,pStatusCallback);
	}
	void Event(odc::ConnectState state, const std::string& SenderName) override
	{
		auto Sendee = Port1;
		if(SenderName == Port1->GetName())
			Sendee = Port2;
		Sendee->Event(state,Name);
	}

private:
	inline void PublishEvent(const std::shared_ptr<const odc::EventInfo> event, const std::shared_ptr<odc::DataPort> Sendee, const odc::SharedStatusCallback_t pStatusCallback)
	{
		if(auto log = odc::spdlog_get("opendatacon"))
			log->trace("{} {} {} Payload {} Event {} => {}", event->GetSourcePort(), ToString(event->GetEventType()),event->GetIndex(), event->GetPayloadString(), Name, Sendee->GetName());
		Sendee->Event(event,Name,pStatusCallback);
	}
	std::shared_ptr<odc::DataPort> Port1;
	std::shared_ptr<odc::DataPort> Port2;
};

#endif // TESTHELPERS_H

