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
 * Block5Test.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  TASE.2 Block 5 (device control): the object encodings and result mapping, the server and client sides against each other in memory
 *  (a fake clock, no sockets), and the control configuration. The device model is that of IEC 60870-6-503 / -802 (select = Read of the
 *  select object, operate = Write of the bare value) and of a real peer's behaviour. Synthetic names only.
 */

#include "StackTestHelpers.h"
#include "Tase2Client.h"
#include "Tase2Control.h"
#include <algorithm>
#include <catch.hpp>
#include <deque>
#include <map>
#include <sstream>

#define SUITE(name) "ICCP block 5 - " name

using namespace iccp;
using iccp::stack::LogLevel;
using tase2::ControlError;
using tase2::ControlType;
using tase2::ControlValue;
namespace DA = iccp::mms::DataAccessError;

// ---------------------------------------------------------------------------------------------------------------------------------
// objects and mappings
// ---------------------------------------------------------------------------------------------------------------------------------

TEST_CASE(SUITE("the Block 5 bit of Supported_Features is the fifth"))
{
	CHECK(static_cast<int>(tase2::Block::Block5) == 4);
	const auto features = tase2::MakeSupportedFeatures({tase2::Block::Block1, tase2::Block::Block2, tase2::Block::Block5});
	CHECK(tase2::HasBlock(features, tase2::Block::Block5));
	CHECK_FALSE(tase2::HasBlock(features, tase2::Block::Block3));
	CHECK_FALSE(tase2::HasBlock(features, tase2::Block::Block4));
	CHECK_FALSE(tase2::HasBlock(tase2::MakeSupportedFeatures({tase2::Block::Block1, tase2::Block::Block2}), tase2::Block::Block5));
}

TEST_CASE(SUITE("device and select objects"))
{
	CHECK(tase2::SelectName("DEV_CMD_01") == "DEV_CMD_01_SBO");
	const auto icc = tase2::SelectObject(mms::ObjectName::Domain("DOMAIN_A", "DEV_CMD_01"));
	CHECK(icc == mms::ObjectName::Domain("DOMAIN_A", "DEV_CMD_01_SBO"));
	const auto vcc = tase2::SelectObject(mms::ObjectName::Vmd("DEV_CMD_01"));
	CHECK(vcc == mms::ObjectName::Vmd("DEV_CMD_01_SBO"));

	//the operate data is the bare value: an integer, or a 32 bit float for a real setpoint
	for(ControlType type : {ControlType::Command, ControlType::DiscreteCommand, ControlType::DiscreteSetpoint})
	{
		ControlValue v;
		v.integer = -1234;
		const mms::Data data = tase2::ToData(type, v);
		CHECK(data.type == mms::DataType::Integer);
		CHECK(tase2::ParseOperateData(type, data) == v);
		//and survives the wire
		CHECK(mms::DecodeData(mms::EncodeData(data)) == data);
	}
	ControlValue real;
	real.real = 12.5;
	const mms::Data rd = tase2::ToData(ControlType::RealSetpoint, real);
	CHECK(rd == mms::Data::Float32(12.5f));
	CHECK(tase2::ParseOperateData(ControlType::RealSetpoint, rd) == real);

	//the select object is an integer
	CHECK(tase2::ParseSelectData(tase2::SelectToData(777)) == 777);
	CHECK(tase2::SelectToData(777).type == mms::DataType::Integer);
	CHECK(tase2::ParseSelectData(tase2::SelectToData(-5)) == -5);
	CHECK(tase2::ParseSelectData(mms::Data::Unsigned(40000)) == 40000);
	CHECK_THROWS_AS(tase2::ParseSelectData(mms::Data::VisibleString("1")), mms::DecodeError);
	CHECK_THROWS_AS(tase2::ParseSelectData(mms::Data::Structure({})), mms::DecodeError);

	//the types: integer of width 16, floating point of 32 bits with an exponent width of 8, and the select object integer of width 16
	using Kind = mms::TypeDescription::Kind;
	for(ControlType type : {ControlType::Command, ControlType::DiscreteCommand, ControlType::DiscreteSetpoint})
	{
		const auto dt = tase2::DeviceTypeDescription(type);
		CHECK(dt.kind == Kind::Integer);
		CHECK(dt.size == 16);
	}
	const auto rt = tase2::DeviceTypeDescription(ControlType::RealSetpoint);
	CHECK(rt.kind == Kind::FloatingPoint);
	CHECK(rt.formatWidth == 32);
	CHECK(rt.exponentWidth == 8);
	const auto st = tase2::SelectTypeDescription();
	CHECK(st.kind == Kind::Integer);
	CHECK(st.size == 16);

	CHECK(tase2::ExpectedControlData(ControlType::RealSetpoint) == "a bare value (a floating point)");
	CHECK(tase2::ExpectedControlData(ControlType::Command) == "a bare value (an integer)");
}

TEST_CASE(SUITE("the requests"))
{
	const auto device = mms::ObjectName::Domain("DOMAIN_A", "DEV_CMD_01");
	//the select is a Read of the select object
	const auto read = tase2::BuildReadCheckBack(device);
	const auto& rl = std::get<mms::ListOfVariable>(read.specification);
	REQUIRE(rl.size() == 1);
	CHECK(rl[0].name == mms::ObjectName::Domain("DOMAIN_A", "DEV_CMD_01_SBO"));

	//the operate is a Write of the device with the bare value as its only data
	ControlValue v;
	v.integer = 1;
	const auto operate = tase2::BuildOperate(device, ControlType::Command, v);
	CHECK(std::get<mms::ListOfVariable>(operate.specification)[0].name == device);
	REQUIRE(operate.data.size() == 1);
	CHECK(operate.data[0] == mms::Data::Integer(1));
	ControlValue sp;
	sp.real = 100;
	const auto setpoint = tase2::BuildOperate(device, ControlType::RealSetpoint, sp);
	REQUIRE(setpoint.data.size() == 1);
	CHECK(setpoint.data[0] == mms::Data::Float32(100));

	//they encode as MMS PDUs and decode again
	const mms::Pdu pdu = mms::ConfirmedRequest{1, operate};
	const mms::Pdu back = mms::Decode(mms::Encode(pdu));
	CHECK(std::get<mms::ConfirmedRequest>(back).service == mms::ConfirmedServiceRequest(operate));
}

TEST_CASE(SUITE("control errors map to data access errors and back to ODC statuses"))
{
	CHECK(tase2::DataAccessErrorOf(ControlError::NotSelected) == DA::TemporarilyUnavailable);
	CHECK(tase2::DataAccessErrorOf(ControlError::Busy) == DA::TemporarilyUnavailable);
	CHECK(tase2::DataAccessErrorOf(ControlError::TypeMismatch) == DA::TypeInconsistent);
	CHECK(tase2::DataAccessErrorOf(ControlError::NotSupported) == DA::TypeUnsupported);
	CHECK(tase2::DataAccessErrorOf(ControlError::OutOfRange) == DA::ObjectAttributeInconsistent);
	CHECK(tase2::DataAccessErrorOf(ControlError::Unknown) == DA::ObjectNonExistent);
	CHECK(tase2::DataAccessErrorOf(ControlError::Refused) == DA::ObjectAccessDenied);
	CHECK(tase2::DataAccessErrorOf(ControlError::Hardware) == DA::HardwareFault);

	using CS = odc::CommandStatus;
	//client: the data access errors a peer answers a Write with
	CHECK(tase2::CommandStatusOf(DA::ObjectInvalidated) == CS::NO_SELECT);
	CHECK(tase2::CommandStatusOf(DA::HardwareFault) == CS::HARDWARE_ERROR);
	CHECK(tase2::CommandStatusOf(DA::TemporarilyUnavailable) == CS::ALREADY_ACTIVE);
	CHECK(tase2::CommandStatusOf(DA::ObjectAccessDenied) == CS::BLOCKED);
	CHECK(tase2::CommandStatusOf(DA::ObjectUndefined) == CS::NOT_SUPPORTED);
	CHECK(tase2::CommandStatusOf(DA::InvalidAddress) == CS::NOT_SUPPORTED);
	CHECK(tase2::CommandStatusOf(DA::TypeUnsupported) == CS::NOT_SUPPORTED);
	CHECK(tase2::CommandStatusOf(DA::ObjectAccessUnsupported) == CS::NOT_SUPPORTED);
	CHECK(tase2::CommandStatusOf(DA::ObjectNonExistent) == CS::NOT_SUPPORTED);
	CHECK(tase2::CommandStatusOf(DA::TypeInconsistent) == CS::FORMAT_ERROR);
	CHECK(tase2::CommandStatusOf(DA::ObjectValueInvalid) == CS::FORMAT_ERROR);
	CHECK(tase2::CommandStatusOf(DA::ObjectAttributeInconsistent) == CS::OUT_OF_RANGE);
	CHECK(tase2::CommandStatusOf(99) == CS::UNDEFINED);

	//server: what downstream answered
	CHECK(tase2::ControlErrorOf(CS::NO_SELECT) == ControlError::NotSelected);
	CHECK(tase2::ControlErrorOf(CS::FORMAT_ERROR) == ControlError::TypeMismatch);
	CHECK(tase2::ControlErrorOf(CS::NOT_SUPPORTED) == ControlError::NotSupported);
	CHECK(tase2::ControlErrorOf(CS::OUT_OF_RANGE) == ControlError::OutOfRange);
	for(CS s : {CS::ALREADY_ACTIVE, CS::TOO_MANY_OPS, CS::PROCESSING_LIMITED})
		CHECK(tase2::ControlErrorOf(s) == ControlError::Busy);
	for(CS s : {CS::LOCAL, CS::DOWNSTREAM_LOCAL, CS::NOT_AUTHORIZED, CS::AUTOMATION_INHIBIT, CS::BLOCKED, CS::BLOCKED_OTHER_MASTER, CS::NON_PARTICIPATING})
		CHECK(tase2::ControlErrorOf(s) == ControlError::Refused);
	for(CS s : {CS::TIMEOUT, CS::HARDWARE_ERROR, CS::DOWNSTREAM_FAIL, CS::CANCELLED, CS::UNDEFINED, CS::ALREADY_COMPLETE})
		CHECK(tase2::ControlErrorOf(s) == ControlError::Hardware);

	//every control error survives the round trip to the status it stands for
	CHECK(tase2::CommandStatusOf(tase2::DataAccessErrorOf(ControlError::Refused)) == CS::BLOCKED);
	CHECK(tase2::CommandStatusOf(tase2::DataAccessErrorOf(ControlError::Hardware)) == CS::HARDWARE_ERROR);

	//CRoB codes
	using CC = odc::ControlCode;
	for(CC c : {CC::LATCH_ON, CC::PULSE_ON, CC::CLOSE_PULSE_ON})
		CHECK(tase2::CommandOnOff(c) == std::optional<bool>(true));
	for(CC c : {CC::LATCH_OFF, CC::PULSE_OFF, CC::TRIP_PULSE_ON})
		CHECK(tase2::CommandOnOff(c) == std::optional<bool>(false));
	CHECK_FALSE(tase2::CommandOnOff(CC::NUL).has_value());
	CHECK_FALSE(tase2::CommandOnOff(CC::UNDEFINED).has_value());

	for(ControlError e : {ControlError::NotSelected, ControlError::Busy, ControlError::TypeMismatch, ControlError::NotSupported,
	                      ControlError::OutOfRange, ControlError::Unknown, ControlError::Refused, ControlError::Hardware})
		CHECK(std::string(tase2::ToString(e)) != "?");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the server and client sides against each other, in memory
// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
//The controls of both ends: the same devices, seen from the server (Serve) and from the client (Operate). Only the server waits a given time for
//downstream (the client's sequence has its default of 15 s unless a test sets it)
std::string ControlsJson(bool server = false)
{
	return std::string(R"("BinaryControls": [
			{"Index": 0, "Name": "DEV_CMD_01", "SelectTimeoutms": 2000)") + (server ? R"(, "CheckBackID": 40000, "Timeoutms": 3000)" : "") + R"(},
			{"Index": 1, "Name": "DEV_CMD_02", "SelectRequired": false, "CommandValueOn": 2, "CommandValueOff": 3},
			{"Index": 2, "Name": "DEV_CMD_03", "Type": "DiscreteCommand", "ControlCodeOn": "CLOSE_PULSE_ON", "ControlCodeOff": "TRIP_PULSE_ON"}],
		"AnalogControls": [
			{"Index": 0, "Name": "DEV_SP_01", "ScaleFactor": 0.5, "Offset": 10},
			{"Index": 1, "Name": "DEV_SP_02", "Type": "DiscreteSetpoint", "AnalogOutputType": "Int16"},
			{"Index": 2, "Name": "DEV_SP_03", "SelectRequired": false, "ScaleFactor": 0.5, "Offset": 10},
			{"Index": 3, "Name": "DEV_SP_04", "Type": "DiscreteSetpoint", "SelectRequired": false}])";
}

ICCPPortConf LoadConf(const std::string& json, bool listener)
{
	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string errs;
	std::istringstream in(json);
	REQUIRE(Json::parseFromStream(builder, in, &root, &errs));
	ICCPPortConf c;
	const auto problems = c.Load(root, listener);
	for(const auto& p : problems)
		UNSCOPED_INFO(p);
	REQUIRE(problems.empty());
	return c;
}

//the kinds of the requests the server saw, "Read DEV_CMD_01_SBO", "Write DEV_CMD_01", ...
std::string KindOf(const mms::ConfirmedServiceRequest& req)
{
	auto first = [](const mms::VariableAccessSpecification& s) -> std::string
			 {
				 const auto* l = std::get_if<mms::ListOfVariable>(&s);
				 return l && !l->empty() ? (*l)[0].name.itemId : std::string("?");
			 };
	if(const auto* r = std::get_if<mms::ReadRequest>(&req))
		return "Read " + first(r->specification);
	if(const auto* w = std::get_if<mms::WriteRequest>(&req))
		return "Write " + first(w->specification);
	return "other";
}

struct B5Fixture
{
	stacktest::Link link;
	ICCPPortConf serverConf;
	ICCPPortConf clientConf;
	std::unique_ptr<tase2::ResponderModel> model;
	std::unique_ptr<tase2::ResponderSession> server;
	std::unique_ptr<tase2::ClientSession> client;
	std::vector<std::pair<LogLevel, std::string>> logs;
	std::string fatal;

	//what the downstream side does with an operate
	std::vector<tase2::ControlOperation> ops;
	std::deque<std::function<void(odc::CommandStatus)>> held; //operates downstream has not answered
	bool hold = false;
	odc::CommandStatus answer = odc::CommandStatus::SUCCESS;
	//what the wire carries, from the moment the client is ready
	std::vector<std::string> wire;
	std::vector<mms::Data> wireData;           //the data of the Writes on the wire
	bool silent = false;                       //the server swallows requests
	std::optional<mms::ServiceError> failWith; //the server answers requests with this error
	bool rejectAll = false;

	B5Fixture(bool serverHasControls = true, const std::string& serverExtra = ""):
		serverConf(LoadConf(serverHasControls ? "{\"ServiceRole\": \"Server\", \"LocalDomain\": \"DOMAIN_A\", \"BilateralTableID\": \"BT_TEST_A\"," + ControlsJson(true) + serverExtra + "}" :
			"{\"ServiceRole\": \"Server\", \"LocalDomain\": \"DOMAIN_A\", \"BilateralTableID\": \"BT_TEST_A\"" + serverExtra + "}", true)),
		clientConf(LoadConf("{\"ServiceRole\": \"Client\", \"RemoteDomain\": \"DOMAIN_A\"," + ControlsJson() + "}", false))
	{
		model = std::make_unique<tase2::ResponderModel>(serverConf, true);
		StartServer();
		link.b.requestHook = [this](uint32_t id, const mms::ConfirmedServiceRequest& req) { return OnServerRequest(id, req); };
		link.Start();
		REQUIRE(link.a.conn.IsAssociated());
	}

	tase2::Timebase Time()
	{
		tase2::Timebase tb;
		tb.steady = [this]() { return link.clock.now; };
		tb.wallMs = []() { return odc::msSinceEpoch_t(1700000000000); };
		return tb;
	}
	void StartServer()
	{
		server = std::make_unique<tase2::ResponderSession>(*model, link.b.conn, Time(), [this](LogLevel l, const std::string& t) { logs.emplace_back(l, t); });
		server->SetControlHandler([this](const tase2::ControlOperation& op, std::function<void(odc::CommandStatus)> done)
			{
				ops.push_back(op);
				if(hold)
					held.push_back(std::move(done));
				else
					done(answer);
			});
	}
	bool OnServerRequest(uint32_t id, const mms::ConfirmedServiceRequest& req)
	{
		if(rejectAll)
		{
			link.b.conn.RejectRequest(id);
			return true;
		}
		if(failWith)
		{
			link.b.conn.RespondError(id, *failWith);
			return true;
		}
		wire.push_back(KindOf(req));
		if(const auto* w = std::get_if<mms::WriteRequest>(&req))
			for(const auto& d : w->data)
				wireData.push_back(d);
		if(silent)
			return true;
		server->OnRequest(id, req);
		return true;
	}

	//the client session: verifies the peer, then is ready
	void StartClient()
	{
		tase2::ClientSession::Callbacks cb;
		cb.onFatal = [this](const std::string& why) { fatal = why; };
		client = std::make_unique<tase2::ClientSession>(clientConf, link.a.conn, Time(), [this](LogLevel l, const std::string& t) { logs.emplace_back(l, t); }, std::move(cb));
		client->OnAssociated();
		link.Pump();
		wire.clear();
	}
	//time passes: timers on both sides
	void Advance(int ms)
	{
		link.clock.now += std::chrono::milliseconds(ms);
		link.a.conn.Tick();
		link.b.conn.Tick();
		server->Tick();
		if(client)
			client->Tick();
		link.Pump();
	}

	size_t Count(const std::string& part, std::optional<LogLevel> level = std::nullopt) const
	{
		size_t n = 0;
		for(const auto& [l, t] : logs)
			if((!level || *level == l) && t.find(part) != std::string::npos)
				++n;
		return n;
	}

	//operate a device of the peer from an ODC command; the result appears in *result when the operation ends
	std::shared_ptr<std::optional<odc::CommandStatus>> Operate(const std::string& name, tase2::ClientSession::ControlCommand command)
	{
		auto result = std::make_shared<std::optional<odc::CommandStatus>>();
		const IccpControlConf* c = clientConf.FindControl(name, ControlDirection::Operate);
		REQUIRE(c != nullptr);
		client->Operate(*c, command, [result](odc::CommandStatus s) { *result = s; });
		link.Pump();
		return result;
	}
	static tase2::ClientSession::ControlCommand On()
	{
		return {true, 0};
	}
	static tase2::ClientSession::ControlCommand Off()
	{
		return {false, 0};
	}
	static tase2::ClientSession::ControlCommand Value(double v)
	{
		return {false, v};
	}

	// ---- raw requests, as a peer that does not follow our client's sequence would send them
	stack::Outcome Request(mms::ConfirmedServiceRequest request)
	{
		stack::Outcome out;
		bool got = false;
		link.a.conn.SendRequest(std::move(request), [&](const stack::Outcome& o)
			{
				out = o;
				got = true;
			});
		link.Pump();
		REQUIRE(got);
		return out;
	}
	static mms::ObjectName Dev(const std::string& name)
	{
		return mms::ObjectName::Domain("DOMAIN_A", name);
	}
	//Read of the select object: the CheckBackID, or the data access error
	std::pair<int64_t, std::optional<int64_t>> ReadSbo(const std::string& name)
	{
		const auto o = Request(tase2::BuildReadCheckBack(Dev(name)));
		const auto* rr = o.Get<mms::ReadResponse>();
		REQUIRE(rr != nullptr);
		REQUIRE(rr->results.size() == 1);
		if(!rr->results[0].success)
			return {0, rr->results[0].error}
		;
		return {tase2::ParseSelectData(rr->results[0].data), std::nullopt};
	}
	//the data access error of a Write, nullopt for success
	std::optional<int64_t> WriteResult(const stack::Outcome& o)
	{
		const auto* wr = o.Get<mms::WriteResponse>();
		REQUIRE(wr != nullptr);
		REQUIRE(wr->results.size() == 1);
		if(wr->results[0].success)
			return std::nullopt;
		return wr->results[0].error;
	}
	std::optional<int64_t> OperateRaw(const std::string& name, ControlType type, int64_t integer, double real = 0)
	{
		ControlValue v;
		v.integer = integer;
		v.real = real;
		return WriteResult(Request(tase2::BuildOperate(Dev(name), type, v)));
	}
	//a Write of the device variable (or of any object) with any data (a bare value, or anything else)
	std::optional<int64_t> WriteBare(const std::string& name, const mms::Data& data)
	{
		mms::WriteRequest w;
		w.specification = mms::ListOfVariable{mms::VariableSpec{Dev(name), std::nullopt}
		};
		w.data.push_back(data);
		return WriteResult(Request(w));
	}
	//a Read of the device variable itself: the data access error (it is write only)
	std::optional<int64_t> ReadDevice(const std::string& name)
	{
		mms::ReadRequest r;
		r.specification = mms::ListOfVariable{mms::VariableSpec{Dev(name), std::nullopt}
		};
		const auto o = Request(r);
		const auto* rr = o.Get<mms::ReadResponse>();
		REQUIRE(rr != nullptr);
		REQUIRE(rr->results.size() == 1);
		if(rr->results[0].success)
			return std::nullopt;
		return rr->results[0].error;
	}
};

using CS = odc::CommandStatus;
}

TEST_CASE(SUITE("select before operate: the Read of the select object selects, the Write of the bare value operates; the downstream control event is as configured"))
{
	B5Fixture f;
	f.StartClient();
	REQUIRE(f.client->Ready());
	REQUIRE(f.fatal.empty());

	const auto r = f.Operate("DEV_CMD_01", B5Fixture::On());
	REQUIRE(r->has_value());
	CHECK(**r == CS::SUCCESS);
	//no Write of the select object: a Read and a Write
	CHECK(f.wire == std::vector<std::string>({"Read DEV_CMD_01_SBO", "Write DEV_CMD_01"}));
	REQUIRE(f.ops.size() == 1);
	CHECK(f.ops[0].control->name == "DEV_CMD_01");
	CHECK(f.ops[0].control->index == 0);
	CHECK(f.ops[0].on);
	CHECK(f.ops[0].control->codeOn == odc::ControlCode::LATCH_ON);

	CHECK(f.client->ControlCounters().selects == 1);
	CHECK(f.client->ControlCounters().operates == 1);
	CHECK(f.client->ControlCounters().failures == 0);
	CHECK(f.server->ControlCounters().selects == 1);
	CHECK(f.server->ControlCounters().operates == 1);
	CHECK(f.server->ControlCounters().refused == 0);
	CHECK(f.Count("control DEV_CMD_01 selected (CheckBackID 40000, for 2000 ms)", LogLevel::Info) == 1);
	CHECK(f.Count("control DEV_CMD_01 operated by the peer: ON", LogLevel::Info) == 1);
	CHECK(f.Count("control DEV_CMD_01 operated on the peer: ON (select before operate)", LogLevel::Info) == 1);

	//the select is used up: a second operate selects again
	f.wire.clear();
	const auto r2 = f.Operate("DEV_CMD_01", B5Fixture::Off());
	CHECK(**r2 == CS::SUCCESS);
	CHECK(f.wire == std::vector<std::string>({"Read DEV_CMD_01_SBO", "Write DEV_CMD_01"}));
	REQUIRE(f.ops.size() == 2);
	CHECK_FALSE(f.ops[1].on);

	//the device itself is write only
	CHECK(f.ReadDevice("DEV_CMD_01") == std::optional<int64_t>(DA::ObjectAccessDenied));
}

TEST_CASE(SUITE("the Read of the select object returns the configured CheckBackID, the same every time and on every association"))
{
	B5Fixture f;
	f.StartClient();
	//DEV_CMD_01 is configured with 40000 (above 32767: the width 16 integer is not forced into the signed range), DEV_SP_01 has the default
	for(int i = 0; i < 3; ++i)
	{
		const auto [id, err] = f.ReadSbo("DEV_CMD_01");
		REQUIRE_FALSE(err.has_value());
		CHECK(id == 40000);
		//let the select lapse (SelectTimeoutms 2000), so that the next Read is a new select
		f.Advance(2100);
	}
	const auto def = f.ReadSbo("DEV_SP_01");
	REQUIRE_FALSE(def.second.has_value());
	CHECK(def.first == 1);
	//a new association: a new session, the same value
	f.StartServer();
	CHECK(f.ReadSbo("DEV_CMD_01").first == 40000);
	//the answer is an Integer of the wire
	const auto o = f.Request(tase2::BuildReadCheckBack(B5Fixture::Dev("DEV_CMD_02")));
	const auto* rr = o.Get<mms::ReadResponse>();
	REQUIRE(rr != nullptr);
	REQUIRE(rr->results.size() == 1);
	CHECK(rr->results[0].data == mms::Data::Integer(1));
	CHECK(mms::DecodeData(mms::EncodeData(mms::Data::Integer(40000))) == mms::Data::Integer(40000));
}

TEST_CASE(SUITE("a select arms the device for its timeout; a second select is refused; the timeout lapses it"))
{
	B5Fixture f;
	f.StartClient();
	CHECK(f.ReadSbo("DEV_CMD_01").first == 40000); //SelectTimeoutms 2000
	CHECK(f.server->ControlCounters().selects == 1);
	//armed: a second select is refused as temporarily unavailable, and says so
	CHECK(f.ReadSbo("DEV_CMD_01").second == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.Count("control DEV_CMD_01: select refused: busy: already armed (data access error temporarily-unavailable", LogLevel::Warn) == 1);
	f.Advance(1900);
	CHECK(f.Count("expired") == 0);
	CHECK(f.ReadSbo("DEV_CMD_01").second == std::optional<int64_t>(DA::TemporarilyUnavailable));
	f.Advance(200);
	CHECK(f.Count("the selection of control DEV_CMD_01 expired", LogLevel::Info) == 1);
	//lapsed: an operate is refused (not armed), and the device can be selected again
	CHECK(f.OperateRaw("DEV_CMD_01", ControlType::Command, 1) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.ops.empty());
	CHECK(f.ReadSbo("DEV_CMD_01").first == 40000);
	CHECK_FALSE(f.OperateRaw("DEV_CMD_01", ControlType::Command, 1).has_value());
	CHECK(f.ops.size() == 1);

	//a lapse that no tick has noticed yet is found by the next Read, and by an operate
	B5Fixture g;
	g.StartClient();
	CHECK(g.ReadSbo("DEV_CMD_01").first == 40000);
	g.link.clock.now += std::chrono::milliseconds(2500);
	CHECK(g.ReadSbo("DEV_CMD_01").first == 40000);
	g.link.clock.now += std::chrono::milliseconds(2500);
	CHECK(g.OperateRaw("DEV_CMD_01", ControlType::Command, 1) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(g.ops.empty());
}

TEST_CASE(SUITE("an operate of a device that needs a select, without one or by another association, is temporarily unavailable"))
{
	B5Fixture f;
	f.StartClient();
	//never selected
	CHECK(f.OperateRaw("DEV_CMD_01", ControlType::Command, 1) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.OperateRaw("DEV_SP_01", ControlType::RealSetpoint, 0, 1.5) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.OperateRaw("DEV_SP_02", ControlType::DiscreteSetpoint, 5) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.ops.empty());
	CHECK(f.Count("operate refused: not selected: the device is not selected (armed) by this association", LogLevel::Warn) == 3);

	//selected by one association; the next one has a new session, in which the device is idle
	CHECK(f.ReadSbo("DEV_CMD_01").first == 40000);
	f.StartServer();
	CHECK(f.OperateRaw("DEV_CMD_01", ControlType::Command, 1) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.ops.empty());

	//the select belongs to the device that was read: another device is not armed by it
	CHECK(f.ReadSbo("DEV_CMD_01").first == 40000);
	CHECK(f.OperateRaw("DEV_SP_01", ControlType::RealSetpoint, 0, 1.5) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.ops.empty());
	//and the select is used up by the operate
	CHECK_FALSE(f.OperateRaw("DEV_CMD_01", ControlType::Command, 1).has_value());
	CHECK(f.OperateRaw("DEV_CMD_01", ControlType::Command, 1) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.ops.size() == 1);
}

TEST_CASE(SUITE("a device that needs no select takes an operate at any time, and a select still arms it (deliberate leniency)"))
{
	B5Fixture f;
	f.StartClient();
	//direct operate: no select, no Read of the select object on the wire
	f.wire.clear();
	CHECK_FALSE(f.OperateRaw("DEV_CMD_02", ControlType::Command, 2).has_value());
	CHECK(f.wire == std::vector<std::string>({"Write DEV_CMD_02"}));
	CHECK(f.server->ControlCounters().selects == 0);
	//a select works as well (a client in select mode), and the operate uses it up
	CHECK(f.ReadSbo("DEV_CMD_02").first == 1);
	CHECK(f.server->ControlCounters().selects == 1);
	CHECK(f.ReadSbo("DEV_CMD_02").second == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK_FALSE(f.OperateRaw("DEV_CMD_02", ControlType::Command, 3).has_value());
	CHECK(f.ReadSbo("DEV_CMD_02").first == 1); //idle again: it can be selected
	CHECK_FALSE(f.OperateRaw("DEV_CMD_02", ControlType::Command, 2).has_value());
	REQUIRE(f.ops.size() == 3);
	CHECK(f.ops[0].on);
	CHECK_FALSE(f.ops[1].on);
	CHECK(f.ops[2].on);
}

TEST_CASE(SUITE("the client side of a device without a select requirement does a direct operate only, and of one with it a Read then a Write"))
{
	B5Fixture f;
	f.StartClient();
	CHECK(**f.Operate("DEV_SP_03", B5Fixture::Value(30)) == CS::SUCCESS); //SelectRequired false
	CHECK(f.wire == std::vector<std::string>({"Write DEV_SP_03"}));
	f.wire.clear();
	CHECK(**f.Operate("DEV_SP_01", B5Fixture::Value(30)) == CS::SUCCESS); //SelectRequired true
	CHECK(f.wire == std::vector<std::string>({"Read DEV_SP_01_SBO", "Write DEV_SP_01"}));
}

TEST_CASE(SUITE("the client checks the CheckBackID against the configured one: match, mismatch, and none configured"))
{
	//the server returns 40000 for DEV_CMD_01
	{
		//match
		B5Fixture f;
		f.clientConf.controls[0].checkBackId = 40000;
		f.StartClient();
		CHECK(**f.Operate("DEV_CMD_01", B5Fixture::On()) == CS::SUCCESS);
		CHECK(f.ops.size() == 1);
	}
	{
		//mismatch: not written, FORMAT_ERROR, a clear line, and the server's device stays armed until its timeout
		B5Fixture f;
		f.clientConf.controls[0].checkBackId = 123;
		f.StartClient();
		const auto r = f.Operate("DEV_CMD_01", B5Fixture::On());
		REQUIRE(r->has_value());
		CHECK(**r == CS::FORMAT_ERROR);
		CHECK(f.wire == std::vector<std::string>({"Read DEV_CMD_01_SBO"}));
		CHECK(f.ops.empty());
		CHECK(f.client->ControlCounters().failures == 1);
		CHECK(f.client->ControlCounters().operates == 0);
		CHECK(f.Count("the select returned CheckBackID 40000 but CheckBackID 123 was expected", LogLevel::Error) == 1);
		CHECK(f.Count("the operate was not sent") == 1);
		//the device is armed on the server: a second try is refused until the select lapses
		CHECK(**f.Operate("DEV_CMD_01", B5Fixture::On()) == CS::ALREADY_ACTIVE);
		f.Advance(2100);
		f.clientConf.controls[0].checkBackId = 40000;
		CHECK(**f.Operate("DEV_CMD_01", B5Fixture::On()) == CS::SUCCESS);
	}
	{
		//none configured: any value is accepted (DEV_SP_01 returns the default 1, DEV_CMD_01 40000)
		B5Fixture f;
		f.StartClient();
		CHECK_FALSE(f.clientConf.controls[0].checkBackId.has_value());
		CHECK(**f.Operate("DEV_CMD_01", B5Fixture::On()) == CS::SUCCESS);
		CHECK(**f.Operate("DEV_SP_01", B5Fixture::Value(12)) == CS::SUCCESS);
	}
	{
		//the expectation is not used for a device that is operated directly
		B5Fixture f;
		f.clientConf.controls[1].checkBackId = 5; //DEV_CMD_02: SelectRequired false
		f.StartClient();
		CHECK(**f.Operate("DEV_CMD_02", B5Fixture::On()) == CS::SUCCESS);
	}
	{
		//a negative one, as a client that reads the width 16 integer as signed would configure
		B5Fixture f;
		f.serverConf.controls[0].checkBackId = -25536; //DEV_CMD_01
		f.clientConf.controls[0].checkBackId = -25536;
		f.StartClient();
		CHECK(**f.Operate("DEV_CMD_01", B5Fixture::On()) == CS::SUCCESS);
		f.clientConf.controls[0].checkBackId = 40000;
		CHECK(**f.Operate("DEV_CMD_01", B5Fixture::On()) == CS::FORMAT_ERROR);
	}
}

TEST_CASE(SUITE("direct operate, with the command values of the device"))
{
	B5Fixture f;
	f.StartClient();
	const auto r = f.Operate("DEV_CMD_02", B5Fixture::On());
	REQUIRE(r->has_value());
	CHECK(**r == CS::SUCCESS);
	CHECK(f.wire == std::vector<std::string>({"Write DEV_CMD_02"}));
	REQUIRE(f.ops.size() == 1);
	CHECK(f.ops[0].on);
	CHECK(f.ops[0].integer == 2); //CommandValueOn
	CHECK(f.client->ControlCounters().selects == 0);
	CHECK(f.Count("control DEV_CMD_02 operated on the peer: ON (direct operate)", LogLevel::Info) == 1);

	const auto r2 = f.Operate("DEV_CMD_02", B5Fixture::Off());
	CHECK(**r2 == CS::SUCCESS);
	CHECK_FALSE(f.ops[1].on);
	CHECK(f.ops[1].integer == 3);

	//a device that needs a select refuses an operate without one
	CHECK(f.OperateRaw("DEV_CMD_01", ControlType::Command, 1) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.ops.size() == 2);
}

TEST_CASE(SUITE("setpoints are scaled both ways, and a discrete setpoint is rounded"))
{
	B5Fixture f;
	f.StartClient();
	//ODC 25 = TASE.2 value * 0.5 + 10: the wire carries 30, downstream gets 25 again
	const auto r = f.Operate("DEV_SP_01", B5Fixture::Value(25));
	CHECK(**r == CS::SUCCESS);
	REQUIRE(f.ops.size() == 1);
	CHECK(f.ops[0].control->name == "DEV_SP_01");
	CHECK(f.ops[0].value == Approx(25.0));
	//what was on the wire: the bare 32 bit float
	CHECK(f.wireData == std::vector<mms::Data>({mms::Data::Float32(30)}));

	const auto r2 = f.Operate("DEV_SP_02", B5Fixture::Value(1234.6));
	CHECK(**r2 == CS::SUCCESS);
	REQUIRE(f.ops.size() == 2);
	CHECK(f.ops[1].integer == 1235);
	CHECK(f.ops[1].value == Approx(1235.0));
	CHECK(f.ops[1].control->analogEvent == odc::EventType::AnalogOutputInt16);
	CHECK(f.wireData.back() == mms::Data::Integer(1235));

	//values the wire cannot carry are refused before anything is sent
	f.wire.clear();
	const auto big = f.Operate("DEV_SP_02", B5Fixture::Value(1e12));
	CHECK(**big == CS::OUT_OF_RANGE);
	const auto nan = f.Operate("DEV_SP_01", B5Fixture::Value(std::numeric_limits<double>::quiet_NaN()));
	CHECK(**nan == CS::FORMAT_ERROR);
	const auto huge = f.Operate("DEV_SP_01", B5Fixture::Value(1e300));
	CHECK(**huge == CS::OUT_OF_RANGE);
	CHECK(f.wire.empty());
	CHECK(f.client->ControlCounters().failures == 3);
}

TEST_CASE(SUITE("what a peer does that is not defined: Write of the select object, Read of the device, and the downstream-less device"))
{
	B5Fixture f;
	f.StartClient();

	//a Write of the select object is not defined: access denied, whatever it carries, and it arms nothing
	CHECK(f.WriteBare("DEV_CMD_01_SBO", mms::Data::Integer(40000)) == std::optional<int64_t>(DA::ObjectAccessDenied));
	CHECK(f.WriteBare("DEV_CMD_01_SBO", mms::Data::VisibleString("x")) == std::optional<int64_t>(DA::ObjectAccessDenied));
	CHECK(f.WriteBare("DEV_CMD_02_SBO", mms::Data::Integer(1)) == std::optional<int64_t>(DA::ObjectAccessDenied));
	CHECK(f.OperateRaw("DEV_CMD_01", ControlType::Command, 1) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.Count("control DEV_CMD_01: write refused: refused: the select object is only read", LogLevel::Warn) == 2);
	//a Read of the device is not defined: the device is write only
	CHECK(f.ReadDevice("DEV_CMD_01") == std::optional<int64_t>(DA::ObjectAccessDenied));
	CHECK(f.ReadDevice("DEV_SP_01") == std::optional<int64_t>(DA::ObjectAccessDenied));
	CHECK(f.ReadDevice("DEV_CMD_02") == std::optional<int64_t>(DA::ObjectAccessDenied));
	CHECK(f.Count("a Read of the device is refused", LogLevel::Warn) == 3);
	CHECK(f.ops.empty());

	//a device that does not exist is refused like any other object (the Write is not allowed)
	{
		mms::WriteRequest w;
		w.specification = mms::ListOfVariable{mms::VariableSpec{B5Fixture::Dev("DEV_NOT_THERE"), std::nullopt}
		};
		w.data.push_back(tase2::ToData(ControlType::Command, ControlValue{}));
		CHECK(f.WriteResult(f.Request(w)) == std::optional<int64_t>(DA::ObjectAccessDenied));
	}
	//a Read of a select object that does not exist
	{
		const auto o = f.Request(tase2::BuildReadCheckBack(B5Fixture::Dev("DEV_NOT_THERE")));
		const auto* rr = o.Get<mms::ReadResponse>();
		REQUIRE(rr != nullptr);
		REQUIRE(rr->results.size() == 1);
		CHECK_FALSE(rr->results[0].success);
		CHECK(rr->results[0].error == DA::ObjectNonExistent);
	}

	//no downstream to carry an operate out: the device cannot be selected (hardware fault)
	B5Fixture g;
	g.StartClient();
	g.server->SetControlHandler(nullptr);
	CHECK(g.ReadSbo("DEV_CMD_01").second == std::optional<int64_t>(DA::HardwareFault));
	CHECK(g.Count("control DEV_CMD_01: select refused: hardware failure", LogLevel::Error) == 1);
}

TEST_CASE(SUITE("the operate data is the bare value of the device's type; anything else is type-inconsistent"))
{
	//V: 100.0f as a peer writes it: FloatingPoint [7], exponent width 8, IEEE 754 single
	const mms::Bytes wire = {0x87, 0x05, 0x08, 0x42, 0xC8, 0x00, 0x00};
	const mms::Data bare = mms::DecodeData(wire);
	CHECK(bare == mms::Data::Float32(100));
	CHECK(mms::EncodeData(tase2::ToData(ControlType::RealSetpoint, ControlValue{0, 100.0})) == wire);
	const ControlValue real = tase2::ParseOperateData(ControlType::RealSetpoint, bare);
	CHECK(real.real == Approx(100.0));
	CHECK(tase2::ParseOperateData(ControlType::RealSetpoint, mms::Data::Float64(-2.5)).real == Approx(-2.5));

	for(ControlType type : {ControlType::Command, ControlType::DiscreteCommand, ControlType::DiscreteSetpoint})
	{
		CHECK(tase2::ParseOperateData(type, mms::Data::Integer(-7)).integer == -7);
		CHECK(tase2::ParseOperateData(type, mms::Data::Unsigned(7)).integer == 7);
		CHECK_THROWS_AS(tase2::ParseOperateData(type, mms::Data::Float32(1)), mms::DecodeError);
		CHECK_THROWS_AS(tase2::ParseOperateData(type, mms::Data::VisibleString("1")), mms::DecodeError);
		CHECK_THROWS_AS(tase2::ParseOperateData(type, mms::Data::Boolean(true)), mms::DecodeError);
		CHECK_THROWS_AS(tase2::ParseOperateData(type, mms::Data::Unsigned(UINT64_MAX)), mms::DecodeError);
		//the structure form of the earlier design is not an operate
		CHECK_THROWS_AS(tase2::ParseOperateData(type, mms::Data::Structure({mms::Data::Integer(1), mms::Data::Integer(0)})), mms::DecodeError);
	}
	CHECK_THROWS_AS(tase2::ParseOperateData(ControlType::RealSetpoint, mms::Data::Integer(1)), mms::DecodeError);
	CHECK_THROWS_AS(tase2::ParseOperateData(ControlType::RealSetpoint, mms::Data::VisibleString("1")), mms::DecodeError);
	CHECK_THROWS_AS(tase2::ParseOperateData(ControlType::RealSetpoint, mms::Data::Structure({mms::Data::Float32(1), mms::Data::Integer(0)})), mms::DecodeError);
	CHECK_THROWS_AS(tase2::ParseOperateData(ControlType::RealSetpoint, mms::Data::Float32(std::numeric_limits<float>::quiet_NaN())), mms::DecodeError);
	CHECK_THROWS_AS(tase2::ParseOperateData(ControlType::RealSetpoint, mms::Data::Float64(std::numeric_limits<double>::infinity())), mms::DecodeError);

	//the words of the refusal log
	CHECK(tase2::DescribeData(mms::Data::Float32(1)) == "a floating point");
	CHECK(tase2::DescribeData(mms::Data::Integer(1)) == "an integer");
	CHECK(tase2::DescribeData(mms::Data::Structure({mms::Data::Integer(1), mms::Data::Integer(1), mms::Data::Integer(1)})) == "a structure of 3 members");
}

TEST_CASE(SUITE("a bare operate of every device type: float for a setpoint, integer for a discrete setpoint and for a command"))
{
	B5Fixture f;
	f.StartClient();

	//RealSetpoint without a select requirement: ODC value = 100 * 0.5 + 10
	CHECK_FALSE(f.WriteBare("DEV_SP_03", mms::Data::Float32(100)).has_value());
	REQUIRE(f.ops.size() == 1);
	CHECK(f.ops[0].control->name == "DEV_SP_03");
	CHECK(f.ops[0].control->index == 2);
	CHECK(f.ops[0].value == Approx(60.0));
	//a 64 bit float is accepted too
	CHECK_FALSE(f.WriteBare("DEV_SP_03", mms::Data::Float64(-20)).has_value());
	REQUIRE(f.ops.size() == 2);
	CHECK(f.ops[1].value == Approx(0.0));

	//DiscreteSetpoint: an integer, or an unsigned
	CHECK_FALSE(f.WriteBare("DEV_SP_04", mms::Data::Integer(-1234)).has_value());
	CHECK_FALSE(f.WriteBare("DEV_SP_04", mms::Data::Unsigned(77)).has_value());
	REQUIRE(f.ops.size() == 4);
	CHECK(f.ops[2].control->name == "DEV_SP_04");
	CHECK(f.ops[2].integer == -1234);
	CHECK(f.ops[2].value == Approx(-1234.0));
	CHECK(f.ops[3].integer == 77);

	//Command (ON = 2, OFF = 3 on DEV_CMD_02)
	CHECK_FALSE(f.WriteBare("DEV_CMD_02", mms::Data::Integer(2)).has_value());
	CHECK_FALSE(f.WriteBare("DEV_CMD_02", mms::Data::Unsigned(3)).has_value());
	REQUIRE(f.ops.size() == 6);
	CHECK(f.ops[4].on);
	CHECK(f.ops[4].integer == 2);
	CHECK_FALSE(f.ops[5].on);
	CHECK(f.ops[5].integer == 3);
	//neither ON nor OFF
	CHECK(f.WriteBare("DEV_CMD_02", mms::Data::Integer(1)) == std::optional<int64_t>(DA::TypeUnsupported));
	CHECK(f.ops.size() == 6);
	CHECK(f.Count("the value 1 is neither ON nor OFF", LogLevel::Warn) == 1);

	//DiscreteCommand (the defaults: ON = 1, OFF = 0, other codes), after its select
	CHECK(f.ReadSbo("DEV_CMD_03").first == 1);
	CHECK_FALSE(f.WriteBare("DEV_CMD_03", mms::Data::Integer(1)).has_value());
	REQUIRE(f.ops.size() == 7);
	CHECK(f.ops[6].on);
	CHECK(f.ReadSbo("DEV_CMD_03").first == 1);
	CHECK(f.WriteBare("DEV_CMD_03", mms::Data::Integer(5)) == std::optional<int64_t>(DA::TypeUnsupported));
	CHECK_FALSE(f.WriteBare("DEV_CMD_03", mms::Data::Integer(0)).has_value()); //the refusal left the device armed
	REQUIRE(f.ops.size() == 8);
	CHECK_FALSE(f.ops[7].on);

	//and the same, after a select, for the devices that need one
	CHECK(f.ReadSbo("DEV_SP_01").first == 1);
	CHECK_FALSE(f.WriteBare("DEV_SP_01", mms::Data::Float32(8)).has_value());
	REQUIRE(f.ops.size() == 9);
	CHECK(f.ops[8].value == Approx(14.0)); //8 * 0.5 + 10
	CHECK(f.ReadSbo("DEV_SP_02").first == 1);
	CHECK_FALSE(f.WriteBare("DEV_SP_02", mms::Data::Integer(9)).has_value());
	REQUIRE(f.ops.size() == 10);
	CHECK(f.ops[9].integer == 9);

	CHECK(f.server->ControlCounters().operates == 10);
	CHECK(f.server->ControlCounters().refused == 2); //DEV_CMD_02 and DEV_CMD_03 each refused a value that is neither ON nor OFF
	CHECK(f.Count("control DEV_SP_03 operated by the peer", LogLevel::Info) == 2);
}

TEST_CASE(SUITE("an operate keeps the rules: busy, the state before the type, the range"))
{
	B5Fixture f;
	f.StartClient();

	//a selected device: a refused operate (wrong kind, neither ON nor OFF) leaves it armed, a good one uses it up
	CHECK(f.ReadSbo("DEV_CMD_01").first == 40000);
	CHECK(f.WriteBare("DEV_CMD_01", mms::Data::Float32(1)) == std::optional<int64_t>(DA::TypeInconsistent));
	CHECK(f.WriteBare("DEV_CMD_01", mms::Data::Integer(9)) == std::optional<int64_t>(DA::TypeUnsupported));
	CHECK(f.ops.empty());
	CHECK_FALSE(f.WriteBare("DEV_CMD_01", mms::Data::Integer(1)).has_value());
	REQUIRE(f.ops.size() == 1);
	CHECK(f.ops[0].on);
	//the state is checked before the type: an unselected device says temporarily-unavailable, not type-inconsistent
	CHECK(f.WriteBare("DEV_CMD_01", mms::Data::Float32(1)) == std::optional<int64_t>(DA::TemporarilyUnavailable));

	//a device without a select requirement that was armed by a Read: the operate uses the select up
	CHECK(f.ReadSbo("DEV_SP_03").first == 1);
	CHECK_FALSE(f.WriteBare("DEV_SP_03", mms::Data::Float32(4)).has_value());
	CHECK(f.ReadSbo("DEV_SP_03").first == 1);

	//a busy device: the downstream operate is in flight, so a second operate is refused
	B5Fixture g;
	g.StartClient();
	g.hold = true;
	mms::WriteRequest w;
	w.specification = mms::ListOfVariable{mms::VariableSpec{B5Fixture::Dev("DEV_SP_03"), std::nullopt}
	};
	w.data.push_back(mms::Data::Float32(1));
	std::optional<stack::Outcome> pending;
	g.link.a.conn.SendRequest(w, [&](const stack::Outcome& out) { pending = out; });
	g.link.Pump();
	REQUIRE(g.ops.size() == 1);
	CHECK_FALSE(pending.has_value()); //downstream has not answered
	CHECK(g.WriteBare("DEV_SP_03", mms::Data::Float32(2)) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(g.ReadSbo("DEV_SP_03").second == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(g.ops.size() == 1);
	REQUIRE(g.held.size() == 1);
	g.held.front()(CS::SUCCESS);
	g.held.clear();
	g.link.Pump();
	CHECK(pending.has_value());
}

TEST_CASE(SUITE("a bare value of the wrong kind, and the refusal log says what came and what was expected"))
{
	B5Fixture f;
	f.StartClient();

	CHECK(f.WriteBare("DEV_SP_03", mms::Data::VisibleString("100")) == std::optional<int64_t>(DA::TypeInconsistent));
	CHECK(f.WriteBare("DEV_SP_03", mms::Data::Integer(100)) == std::optional<int64_t>(DA::TypeInconsistent));
	CHECK(f.WriteBare("DEV_SP_03", mms::Data::Float32(std::numeric_limits<float>::infinity())) == std::optional<int64_t>(DA::TypeInconsistent));
	CHECK(f.WriteBare("DEV_CMD_02", mms::Data::Float32(1)) == std::optional<int64_t>(DA::TypeInconsistent));
	CHECK(f.WriteBare("DEV_CMD_02", mms::Data::VisibleString("1")) == std::optional<int64_t>(DA::TypeInconsistent));
	CHECK(f.WriteBare("DEV_SP_04", mms::Data::Float32(1)) == std::optional<int64_t>(DA::TypeInconsistent));
	CHECK(f.WriteBare("DEV_SP_04", mms::Data::Structure({mms::Data::Integer(1), mms::Data::Integer(1), mms::Data::Integer(1)})) == std::optional<int64_t>(DA::TypeInconsistent));
	//the structure { Value, CheckBackID } is not an operate
	CHECK(f.WriteBare("DEV_CMD_02", mms::Data::Structure({mms::Data::Integer(1), mms::Data::Integer(0)})) == std::optional<int64_t>(DA::TypeInconsistent));
	CHECK(f.ops.empty());
	CHECK(f.server->ControlCounters().refused == 8);

	CHECK(f.Count("control DEV_SP_03: operate refused: wrong type of value: got a visible string, expected a bare value (a floating point) (data access error", LogLevel::Warn) == 1);
	CHECK(f.Count("control DEV_SP_03: operate refused: wrong type of value: got an integer, expected a bare value (a floating point)") == 1);
	CHECK(f.Count("control DEV_CMD_02: operate refused: wrong type of value: got a floating point, expected a bare value (an integer)") == 1);
	CHECK(f.Count("control DEV_SP_04: operate refused: wrong type of value: got a structure of 3 members, expected") == 1);
	CHECK(f.Count("control DEV_CMD_02: operate refused: wrong type of value: got a structure of 2 members, expected a bare value (an integer)") == 1);
}

TEST_CASE(SUITE("a busy device: the downstream operate in flight"))
{
	B5Fixture f;
	f.hold = true;
	f.StartClient();
	const auto first = f.Operate("DEV_CMD_01", B5Fixture::On());
	REQUIRE(f.ops.size() == 1);
	CHECK_FALSE(first->has_value()); //not answered yet
	//the client does not start another on the same device
	const auto second = f.Operate("DEV_CMD_01", B5Fixture::Off());
	REQUIRE(second->has_value());
	CHECK(**second == CS::ALREADY_ACTIVE);
	//and a peer that does gets temporarily-unavailable: for the select (the Read of the select object) and for the operate
	CHECK(f.ReadSbo("DEV_CMD_01").second == std::optional<int64_t>(DA::TemporarilyUnavailable));
	CHECK(f.OperateRaw("DEV_CMD_01", ControlType::Command, 1) == std::optional<int64_t>(DA::TemporarilyUnavailable));
	//the first one finishes
	REQUIRE(f.held.size() == 1);
	f.held.front()(CS::SUCCESS);
	f.link.Pump();
	REQUIRE(first->has_value());
	CHECK(**first == CS::SUCCESS);
	//a late second answer is ignored
	f.held.front()(CS::HARDWARE_ERROR);
	CHECK(**first == CS::SUCCESS);
	//free again
	f.hold = false;
	const auto third = f.Operate("DEV_CMD_01", B5Fixture::Off());
	CHECK(**third == CS::SUCCESS);
}

TEST_CASE(SUITE("operates on different devices run side by side and finish in any order"))
{
	B5Fixture f;
	f.hold = true;
	f.StartClient();
	const auto a = f.Operate("DEV_CMD_01", B5Fixture::On());
	const auto b = f.Operate("DEV_SP_01", B5Fixture::Value(12));
	const auto c = f.Operate("DEV_CMD_02", B5Fixture::On());
	REQUIRE(f.held.size() == 3);
	CHECK_FALSE(a->has_value());
	CHECK_FALSE(b->has_value());
	CHECK_FALSE(c->has_value());
	//the order of the operates is that of the requests; answer them the other way round, with different results
	f.held[2](CS::SUCCESS);
	f.link.Pump();
	CHECK(**c == CS::SUCCESS);
	CHECK_FALSE(a->has_value());
	f.held[0](CS::BLOCKED);
	f.link.Pump();
	CHECK(**a == CS::BLOCKED);
	f.held[1](CS::SUCCESS);
	f.link.Pump();
	CHECK(**b == CS::SUCCESS);
	CHECK(f.client->ControlCounters().operates == 2);
	CHECK(f.client->ControlCounters().failures == 1);
}

TEST_CASE(SUITE("downstream failures reach the client as the matching ODC status"))
{
	const std::pair<CS, CS> table[] = {
		{CS::NO_SELECT, CS::ALREADY_ACTIVE}, {CS::FORMAT_ERROR, CS::FORMAT_ERROR}, {CS::NOT_SUPPORTED, CS::NOT_SUPPORTED}, {CS::OUT_OF_RANGE, CS::OUT_OF_RANGE},
		{CS::ALREADY_ACTIVE, CS::ALREADY_ACTIVE}, {CS::TOO_MANY_OPS, CS::ALREADY_ACTIVE}, {CS::PROCESSING_LIMITED, CS::ALREADY_ACTIVE},
		{CS::LOCAL, CS::BLOCKED}, {CS::DOWNSTREAM_LOCAL, CS::BLOCKED}, {CS::NOT_AUTHORIZED, CS::BLOCKED}, {CS::AUTOMATION_INHIBIT, CS::BLOCKED},
		{CS::BLOCKED, CS::BLOCKED}, {CS::BLOCKED_OTHER_MASTER, CS::BLOCKED}, {CS::NON_PARTICIPATING, CS::BLOCKED},
		{CS::TIMEOUT, CS::HARDWARE_ERROR}, {CS::HARDWARE_ERROR, CS::HARDWARE_ERROR}, {CS::DOWNSTREAM_FAIL, CS::HARDWARE_ERROR}, {CS::CANCELLED, CS::HARDWARE_ERROR},
		{CS::UNDEFINED, CS::HARDWARE_ERROR}
	};
	for(const auto& [downstream, expected] : table)
	{
		B5Fixture f;
		f.answer = downstream;
		f.StartClient();
		const auto r = f.Operate("DEV_CMD_01", B5Fixture::On());
		INFO("downstream " << odc::ToString(downstream));
		REQUIRE(r->has_value());
		CHECK(**r == expected);
		CHECK(f.client->ControlCounters().failures == 1);
		CHECK(f.client->ControlCounters().operates == 0);
		CHECK(f.server->ControlCounters().operates == 0);
		CHECK(f.server->ControlCounters().refused == 1);
		CHECK(f.Count("failed", LogLevel::Error) >= 1);
		//the device is free again and a later operate works
		f.answer = CS::SUCCESS;
		CHECK(**f.Operate("DEV_CMD_01", B5Fixture::On()) == CS::SUCCESS);
	}
}

TEST_CASE(SUITE("downstream that never answers: the peer's Write is answered anyway"))
{
	B5Fixture f;
	f.hold = true;
	f.StartClient();
	const auto r = f.Operate("DEV_CMD_01", B5Fixture::On()); //Timeoutms 3000
	CHECK_FALSE(r->has_value());
	f.Advance(2900);
	CHECK_FALSE(r->has_value());
	f.Advance(200);
	REQUIRE(r->has_value());
	CHECK(**r == CS::HARDWARE_ERROR);
	CHECK(f.Count("downstream did not answer the operate within 3000 ms", LogLevel::Error) == 1);
	//the late answer is ignored, the device is free
	REQUIRE(f.held.size() == 1);
	f.held.front()(CS::SUCCESS);
	f.link.Pump();
	CHECK(**r == CS::HARDWARE_ERROR);
	f.hold = false;
	CHECK(**f.Operate("DEV_CMD_01", B5Fixture::On()) == CS::SUCCESS);
}

TEST_CASE(SUITE("a peer that does not answer or answers with errors"))
{
	{
		//no answer at all: the request times out (5 s), before the 15 s of the sequence
		B5Fixture f;
		f.StartClient();
		f.silent = true;
		const auto r = f.Operate("DEV_CMD_02", B5Fixture::On());
		CHECK_FALSE(r->has_value());
		f.Advance(5100);
		REQUIRE(r->has_value());
		CHECK(**r == CS::TIMEOUT);
		CHECK(f.client->ControlCounters().failures == 1);
		CHECK(f.Count("failed: operate: timed out", LogLevel::Error) == 1);
	}
	{
		//the sequence has its own limit
		B5Fixture f;
		f.StartClient();
		f.silent = true;
		f.clientConf.controls[0].timeoutMs = 3000; //DEV_CMD_01: the sequence is given 3 s
		const auto* c = f.clientConf.FindControl("DEV_CMD_01", ControlDirection::Operate);
		REQUIRE(c != nullptr);
		CHECK(c->EffectiveTimeoutMs() == 3000);
		const auto r = f.Operate("DEV_CMD_01", B5Fixture::On());
		f.link.clock.now += std::chrono::milliseconds(3100);
		f.client->Tick(); //only the client's own timer: the request timeout is 5 s
		REQUIRE(r->has_value());
		CHECK(**r == CS::TIMEOUT);
		CHECK(f.Count("no answer within 3000 ms", LogLevel::Error) == 1);
	}
	{
		//a service error
		B5Fixture f;
		f.StartClient();
		mms::ServiceError e;
		e.errorClass = mms::ErrorClass::Definition;
		e.errorCode = 1;
		f.failWith = e;
		const auto r = f.Operate("DEV_CMD_02", B5Fixture::On());
		CHECK(**r == CS::NOT_SUPPORTED);
		CHECK(f.Count("service error: definition [2] / object-undefined [1]", LogLevel::Error) == 1);
		e.errorClass = mms::ErrorClass::Resource;
		e.errorCode = 1;
		f.failWith = e;
		CHECK(**f.Operate("DEV_CMD_02", B5Fixture::On()) == CS::HARDWARE_ERROR);
	}
	{
		//a reject
		B5Fixture f;
		f.StartClient();
		f.rejectAll = true;
		const auto r = f.Operate("DEV_CMD_02", B5Fixture::On());
		CHECK(**r == CS::NOT_SUPPORTED);
		CHECK(f.Count("rejected: confirmed-requestPDU [1] / unrecognized-service [1]", LogLevel::Error) == 1);
	}
	{
		//an error on the Read of the CheckBackID, in the select, and in the operate
		B5Fixture f;
		f.StartClient();
		mms::ServiceError e;
		e.errorClass = mms::ErrorClass::Access;
		e.errorCode = 3;
		f.failWith = e;
		CHECK(**f.Operate("DEV_CMD_01", B5Fixture::On()) == CS::NOT_SUPPORTED);
		CHECK(f.wire.empty()); //the hook answered before the recording
	}
}

TEST_CASE(SUITE("the peer answers the select or the operate with a data access error: each maps to its status"))
{
	const std::pair<int64_t, CS> table[] = {
		{DA::ObjectInvalidated, CS::NO_SELECT}, {DA::ObjectAccessDenied, CS::BLOCKED}, {DA::TemporarilyUnavailable, CS::ALREADY_ACTIVE},
		{DA::ObjectNonExistent, CS::NOT_SUPPORTED}, {DA::ObjectValueInvalid, CS::FORMAT_ERROR}, {DA::ObjectAttributeInconsistent, CS::OUT_OF_RANGE},
		{DA::HardwareFault, CS::HARDWARE_ERROR}
	};
	for(const auto& [tableError, expected] : table)
		for(const char* step : {"read", "operate"})
		{
			B5Fixture f;
			const int64_t daError = tableError;
			f.StartClient();
			//a scripted peer: serve the sequence by hand, fail at `step`
			f.link.b.requestHook = [&f, daError, step](uint32_t id, const mms::ConfirmedServiceRequest& req)
						     {
							     auto& conn = f.link.b.conn;
							     if(std::get_if<mms::ReadRequest>(&req))
							     {
								     mms::ReadResponse rr;
								     rr.results.push_back(std::string(step) == "read" ? mms::AccessResult::Failure(daError) : mms::AccessResult::Success(tase2::SelectToData(77)));
								     conn.Respond(id, rr);
								     return true;
							     }
							     mms::WriteResponse wr;
							     wr.results.push_back(std::string(step) == "operate" ? mms::WriteResult{false, daError} : mms::WriteResult{true, 0});
							     conn.Respond(id, wr);
							     return true;
						     };
			const auto r = f.Operate("DEV_CMD_01", B5Fixture::On());
			INFO("error " << daError << " at " << step);
			REQUIRE(r->has_value());
			CHECK(**r == expected);
			CHECK(f.client->ControlCounters().failures == 1);
		}
}

TEST_CASE(SUITE("answers that are not right: wrong data, wrong counts"))
{
	auto script = [](B5Fixture& f, std::function<mms::ConfirmedServiceResponse(const mms::ConfirmedServiceRequest&)> answer)
			  {
				  f.link.b.requestHook = [&f, answer](uint32_t id, const mms::ConfirmedServiceRequest& req)
								 {
									 f.link.b.conn.Respond(id, answer(req));
									 return true;
								 };
			  };
	{
		B5Fixture f;
		f.StartClient();
		script(f, [](const mms::ConfirmedServiceRequest&) { mms::ReadResponse rr; rr.results.push_back(mms::AccessResult::Success(mms::Data::VisibleString("x"))); return rr; });
		CHECK(**f.Operate("DEV_CMD_01", B5Fixture::On()) == CS::FORMAT_ERROR);
	}
	{
		//no range is imposed on the CheckBackID: 0, negative and above 32767 are all taken when none is expected
		for(int64_t id : {int64_t(0), int64_t(-3), int64_t(32768), int64_t(65535)})
		{
			B5Fixture f;
			f.StartClient();
			bool written = false;
			f.link.b.requestHook = [&f, id, &written](uint32_t rid, const mms::ConfirmedServiceRequest& req)
						     {
							     if(std::get_if<mms::ReadRequest>(&req))
							     {
								     mms::ReadResponse rr;
								     rr.results.push_back(mms::AccessResult::Success(tase2::SelectToData(id)));
								     f.link.b.conn.Respond(rid, rr);
							     }
							     else
							     {
								     written = true;
								     mms::WriteResponse wr;
								     wr.results.push_back(mms::WriteResult{true, 0});
								     f.link.b.conn.Respond(rid, wr);
							     }
							     return true;
						     };
			INFO("CheckBackID " << id);
			CHECK(**f.Operate("DEV_CMD_01", B5Fixture::On()) == CS::SUCCESS);
			CHECK(written);
		}
	}
	{
		B5Fixture f;
		f.StartClient();
		script(f, [](const mms::ConfirmedServiceRequest&) { return mms::ReadResponse{}; }); //no results
		CHECK(**f.Operate("DEV_CMD_01", B5Fixture::On()) == CS::FORMAT_ERROR);
	}
	{
		B5Fixture f;
		f.StartClient();
		script(f, [](const mms::ConfirmedServiceRequest&) { return mms::WriteResponse{}; }); //no results
		CHECK(**f.Operate("DEV_CMD_02", B5Fixture::On()) == CS::FORMAT_ERROR);
	}
	{
		B5Fixture f;
		f.StartClient();
		script(f, [](const mms::ConfirmedServiceRequest&) { return mms::IdentifyResponse{"V", "M", "R", std::nullopt}; }); //the wrong kind of response
		CHECK(**f.Operate("DEV_CMD_02", B5Fixture::On()) == CS::FORMAT_ERROR);
		CHECK(f.Count("unexpected kind of response") == 1);
	}
}

TEST_CASE(SUITE("the association ends with a select or an operate pending"))
{
	{
		//the server forgets a selection with the association
		B5Fixture f;
		f.StartClient();
		const auto id = f.ReadSbo("DEV_CMD_01").first;
		CHECK(id == 40000);
		f.StartServer(); //a new association: a new session, with no selection
		CHECK(f.OperateRaw("DEV_CMD_01", ControlType::Command, 1) == std::optional<int64_t>(DA::TemporarilyUnavailable));
		CHECK(f.ops.empty());
	}
	{
		//the client's operate in flight when the transport goes: the command ends UNDEFINED, once
		B5Fixture f;
		f.hold = true;
		f.StartClient();
		size_t calls = 0;
		std::optional<CS> status;
		const IccpControlConf* c = f.clientConf.FindControl("DEV_CMD_01", ControlDirection::Operate);
		f.client->Operate(*c, B5Fixture::On(), [&](CS s)
			{
				++calls;
				status = s;
			});
		f.link.Pump();
		REQUIRE(f.held.size() == 1);
		f.link.a.conn.OnTransportClosed();
		REQUIRE(status.has_value());
		CHECK(*status == CS::UNDEFINED);
		CHECK(calls == 1);
		//the downstream answer that comes later changes nothing
		f.held.front()(CS::SUCCESS);
		CHECK(calls == 1);
		//and a command now is refused at once
		const auto r = f.Operate("DEV_CMD_01", B5Fixture::On());
		CHECK(**r == CS::UNDEFINED);
	}
	{
		//the session goes while a command is pending: the command ends, once
		B5Fixture f;
		f.hold = true;
		f.StartClient();
		size_t calls = 0;
		std::optional<CS> status;
		const IccpControlConf* c = f.clientConf.FindControl("DEV_CMD_02", ControlDirection::Operate);
		f.client->Operate(*c, B5Fixture::On(), [&](CS s)
			{
				++calls;
				status = s;
			});
		f.link.Pump();
		f.client.reset();
		REQUIRE(status.has_value());
		CHECK(*status == CS::UNDEFINED);
		CHECK(calls == 1);
	}
	{
		//a shutdown ends it too
		B5Fixture f;
		f.hold = true;
		f.StartClient();
		std::optional<CS> status;
		const IccpControlConf* c = f.clientConf.FindControl("DEV_CMD_02", ControlDirection::Operate);
		f.client->Operate(*c, B5Fixture::On(), [&](CS s) { status = s; });
		f.link.Pump();
		bool done = false;
		f.client->Shutdown([&]() { done = true; });
		CHECK(done);
		REQUIRE(status.has_value());
		CHECK(*status == CS::UNDEFINED);
	}
}

TEST_CASE(SUITE("the server answers once even when downstream answers at once, and without a handler it does not operate"))
{
	B5Fixture f;
	f.answer = CS::SUCCESS;
	f.StartClient();
	CHECK(**f.Operate("DEV_CMD_02", B5Fixture::On()) == CS::SUCCESS); //done() called before the handler returned

	B5Fixture g;
	g.StartClient();
	g.server->SetControlHandler(nullptr);
	CHECK(**g.Operate("DEV_CMD_02", B5Fixture::On()) == CS::NOT_SUPPORTED);
	CHECK(g.server->ControlCounters().refused == 1);
}

TEST_CASE(SUITE("a peer can discover the devices: names, types, the state"))
{
	B5Fixture f;
	f.StartClient();

	mms::GetNameListRequest nl;
	nl.objectClass = mms::ObjectClass::NamedVariable;
	nl.scope = mms::ObjectScope::DomainSpecific;
	nl.domainId = "DOMAIN_A";
	const auto o = f.Request(nl);
	const auto* resp = o.Get<mms::GetNameListResponse>();
	REQUIRE(resp != nullptr);
	const auto& names = resp->identifiers;
	for(const char* n : {"DEV_CMD_01", "DEV_CMD_01_SBO", "DEV_CMD_02", "DEV_CMD_02_SBO", "DEV_CMD_03", "DEV_CMD_03_SBO", "DEV_SP_01", "DEV_SP_01_SBO", "DEV_SP_02", "DEV_SP_02_SBO"})
		CHECK(std::find(names.begin(), names.end(), n) != names.end());

	mms::GetVariableAccessAttributesRequest ga;
	ga.name = B5Fixture::Dev("DEV_SP_01");
	const auto go = f.Request(ga);
	const auto* gr = go.Get<mms::GetVariableAccessAttributesResponse>();
	REQUIRE(gr != nullptr);
	CHECK(gr->typeDescription == tase2::DeviceTypeDescription(ControlType::RealSetpoint));
	ga.name = B5Fixture::Dev("DEV_CMD_01_SBO");
	const auto so = f.Request(ga);
	CHECK(so.Get<mms::GetVariableAccessAttributesResponse>()->typeDescription == tase2::SelectTypeDescription());
	ga.name = B5Fixture::Dev("DEV_CMD_01_NOT_THERE");
	CHECK(f.Request(ga).kind == stack::Outcome::Kind::Error);

	//the peer's features say block 5 (the server serves devices)
	mms::ReadRequest feat;
	feat.specification = mms::ListOfVariable{mms::VariableSpec{tase2::VccName(tase2::Name::SupportedFeatures), std::nullopt}
	};
	const auto fo = f.Request(feat);
	const auto bits = tase2::ParseSupportedFeatures(fo.Get<mms::ReadResponse>()->results[0].data);
	CHECK(tase2::HasBlock(bits, tase2::Block::Block5));
	CHECK(tase2::HasBlock(bits, tase2::Block::Block1));
}

TEST_CASE(SUITE("a client that operates devices drops the association if the peer does not do block 5"))
{
	B5Fixture f(false); //a server without devices: it does not advertise block 5
	f.StartClient();
	CHECK_FALSE(f.client->Ready());
	CHECK(f.fatal == "the peer does not support TASE.2 block 5");
	CHECK(f.Count("the peer does not support TASE.2 block 5", LogLevel::Error) == 1);
	//and a command is refused
	const IccpControlConf* c = f.clientConf.FindControl("DEV_CMD_02", ControlDirection::Operate);
	std::optional<CS> status;
	f.client->Operate(*c, B5Fixture::On(), [&](CS s) { status = s; });
	REQUIRE(status.has_value());
	CHECK(*status == CS::UNDEFINED);
}

TEST_CASE(SUITE("the control counters"))
{
	B5Fixture f;
	f.StartClient();
	CHECK(**f.Operate("DEV_CMD_01", B5Fixture::On()) == CS::SUCCESS);
	CHECK(**f.Operate("DEV_CMD_02", B5Fixture::On()) == CS::SUCCESS);
	f.answer = CS::BLOCKED;
	CHECK(**f.Operate("DEV_SP_01", B5Fixture::Value(1)) == CS::BLOCKED);
	CHECK(f.client->ControlCounters().selects == 2);
	CHECK(f.client->ControlCounters().operates == 2);
	CHECK(f.client->ControlCounters().failures == 1);
	CHECK(f.server->ControlCounters().selects == 2);
	CHECK(f.server->ControlCounters().operates == 2);
	CHECK(f.server->ControlCounters().refused == 1);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// configuration
// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
std::vector<std::string> LoadProblems(const std::string& json, bool listener, ICCPPortConf* out = nullptr)
{
	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string errs;
	std::istringstream in(json);
	REQUIRE(Json::parseFromStream(builder, in, &root, &errs));
	ICCPPortConf c;
	auto problems = c.Load(root, listener);
	if(out)
		*out = c;
	return problems;
}
bool Mentions(const std::vector<std::string>& problems, const std::string& part)
{
	return std::any_of(problems.begin(), problems.end(), [&](const std::string& p) { return p.find(part) != std::string::npos; });
}
}

TEST_CASE(SUITE("configuration: the defaults follow the service role, the keys and their defaults"))
{
	ICCPPortConf c;
	const auto problems = LoadProblems(R"({"ServiceRole": "Server", "LocalDomain": "DOMAIN_A",
		"BinaryControls": [{"Index": 3, "Name": "DEV_CMD_01"}],
		"AnalogControls": [{"Index": 4, "Name": "DEV_SP_01"}]})", true, &c);
	REQUIRE(problems.empty());
	REQUIRE(c.controls.size() == 2);
	const auto& b = c.controls[0];
	CHECK(b.kind == ControlKind::Binary);
	CHECK(b.direction == ControlDirection::Serve);
	CHECK(b.index == 3);
	CHECK(b.name == "DEV_CMD_01");
	CHECK(b.domain == "DOMAIN_A"); //LocalDomain for a served device
	CHECK(b.scope == tase2::Scope::Icc);
	CHECK(b.type == ControlType::Command);
	CHECK(b.selectRequired);
	CHECK(b.selectTimeoutMs == 10000);
	CHECK_FALSE(b.checkBackId.has_value()); //Operate: any is accepted; Serve: DefaultCheckBackId
	CHECK(b.ServedCheckBackId() == 1);
	CHECK(b.timeoutMs == 0);
	CHECK(b.EffectiveTimeoutMs() == 4000); //Serve: waiting for downstream
	CHECK(b.commandValueOn == 1);
	CHECK(b.commandValueOff == 0);
	CHECK(b.codeOn == odc::ControlCode::LATCH_ON);
	CHECK(b.codeOff == odc::ControlCode::LATCH_OFF);
	const auto& a = c.controls[1];
	CHECK(a.kind == ControlKind::Analog);
	CHECK(a.type == ControlType::RealSetpoint);
	CHECK(a.analogEvent == odc::EventType::AnalogOutputDouble64);
	CHECK(a.scale == 1.0);
	CHECK(a.offset == 0.0);
	//a Server serving devices advertises block 5
	CHECK(c.supportedBlocks.count(5) == 1);
	CHECK(c.supportedBlocks.count(1) == 1);
	CHECK(c.FindControl(ControlKind::Binary, 3, ControlDirection::Serve) == &c.controls[0]);
	CHECK(c.FindControl(ControlKind::Binary, 3, ControlDirection::Operate) == nullptr);
	CHECK(c.FindControl("DEV_SP_01", ControlDirection::Serve) == &c.controls[1]);
	CHECK(c.HasControls(ControlDirection::Serve));
	CHECK_FALSE(c.HasControls(ControlDirection::Operate));

	ICCPPortConf k;
	REQUIRE(LoadProblems(R"({"ServiceRole": "Client", "RemoteDomain": "DOMAIN_B",
		"BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Type": "DiscreteCommand", "SelectRequired": false, "SelectTimeoutMs": 500, "Timeoutms": 2500,
			"CommandValueOn": 5, "CommandValueOff": -5}],
		"AnalogControls": [{"Index": 0, "Name": "DEV_SP_01", "Type": "DiscreteSetpoint", "ScaleFactor": 2, "Offset": 1, "Scope": "VCC"}]})", false, &k).empty());
	const auto& kb = k.controls[0];
	CHECK(kb.direction == ControlDirection::Operate); //the default of a Client
	CHECK(kb.domain == "DOMAIN_B");                   //RemoteDomain
	CHECK(kb.type == ControlType::DiscreteCommand);
	CHECK_FALSE(kb.selectRequired);
	CHECK(kb.selectTimeoutMs == 500); //the "...Ms" spelling is accepted too
	CHECK(kb.timeoutMs == 2500);
	CHECK(kb.EffectiveTimeoutMs() == 2500);
	CHECK(kb.commandValueOn == 5);
	CHECK(kb.commandValueOff == -5);
	const auto& ka = k.controls[1];
	CHECK(ka.scope == tase2::Scope::Vcc);
	CHECK(ka.type == ControlType::DiscreteSetpoint);
	CHECK(ka.analogEvent == odc::EventType::AnalogOutputInt32); //a discrete setpoint publishes an integer
	CHECK(ka.scale == 2.0);
	CHECK(ka.offset == 1.0);
	CHECK(ka.EffectiveTimeoutMs() == 15000); //Operate: the whole sequence
	//a Client operating devices requires block 5 of the peer
	CHECK(k.requiredRemoteBlocks.count(5) == 1);
	CHECK(k.requiredRemoteBlocks.count(1) == 1);
}

TEST_CASE(SUITE("configuration: one file under either port type, the control keys of every kind"))
{
	const std::string peer = R"({"ServiceRole": "Peer", "LocalDomain": "DOMAIN_A", "RemoteDomain": "DOMAIN_B",
		"BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Direction": "Serve"}, {"Index": 0, "Name": "DEV_CMD_01", "Direction": "Operate"}],
		"AnalogControls": [{"Index": 0, "Name": "DEV_SP_01", "Direction": "Serve", "AnalogOutputType": "Float32", "ControlCodeOn": "PULSE_ON"},
			{"Index": 0, "Name": "DEV_SP_01", "Direction": "Operate"}]})";
	ICCPPortConf a, b;
	const auto pa = LoadProblems(peer, false, &a);
	const auto pb = LoadProblems(peer, true, &b);
	for(const auto& p : pa)
		UNSCOPED_INFO(p);
	CHECK(pa.empty());
	CHECK(pb.empty());
	REQUIRE(a.controls.size() == 4);
	REQUIRE(b.controls.size() == 4);
	//the same names and index twice: once per direction is allowed
	CHECK(a.controls[0].domain == "DOMAIN_A");
	CHECK(a.controls[1].domain == "DOMAIN_B");
	CHECK(a.controls[2].analogEvent == odc::EventType::AnalogOutputFloat32);
	CHECK(a.supportedBlocks.count(5) == 1);
	CHECK(a.requiredRemoteBlocks.count(5) == 1);
	for(size_t i = 0; i < 4; ++i)
	{
		CHECK(a.controls[i].direction == b.controls[i].direction);
		CHECK(a.controls[i].domain == b.controls[i].domain);
	}
}

TEST_CASE(SUITE("configuration: the direction rules name the rule and the fix"))
{
	//Peer: Direction is required
	auto p = LoadProblems(R"({"ServiceRole": "Peer", "LocalDomain": "D", "RemoteDomain": "E", "BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01"}]})", false);
	REQUIRE(p.size() == 1);
	CHECK(Mentions(p, "\"Direction\" is required with ServiceRole \"Peer\""));
	CHECK(Mentions(p, "\"Serve\" for a device this port owns"));
	CHECK(Mentions(p, "\"Operate\""));

	//a Client cannot serve, a Server cannot operate
	p = LoadProblems(R"({"ServiceRole": "Client", "RemoteDomain": "E", "BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Direction": "Serve"}]})", false);
	CHECK(Mentions(p, "\"Direction\" \"Serve\" needs ServiceRole \"Server\" or \"Peer\""));
	p = LoadProblems(R"({"ServiceRole": "Server", "LocalDomain": "D", "BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Direction": "Operate"}]})", true);
	CHECK(Mentions(p, "\"Direction\" \"Operate\" needs ServiceRole \"Client\" or \"Peer\""));
	//"Read" is not a control direction
	p = LoadProblems(R"({"ServiceRole": "Peer", "LocalDomain": "D", "RemoteDomain": "E", "BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Direction": "Read"}]})", false);
	CHECK(Mentions(p, "\"Direction\" must be \"Serve\""));
	CHECK(Mentions(p, "or \"Operate\""));
	//the port type does not change any of it: the default role does
	p = LoadProblems(R"({"BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Direction": "Operate"}], "RemoteDomain": "E"})", false);
	CHECK(p.empty()); //an ICCPClient defaults to Client
	p = LoadProblems(R"({"BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Direction": "Operate"}], "RemoteDomain": "E"})", true);
	CHECK(Mentions(p, "needs ServiceRole \"Client\" or \"Peer\"")); //an ICCPServer defaults to Server
}

TEST_CASE(SUITE("configuration: validation"))
{
	const std::string head = R"({"ServiceRole": "Server", "LocalDomain": "DOMAIN_A", )";
	auto bad = [&](const std::string& body, const std::string& expect)
		     {
			     const auto p = LoadProblems(head + body + "}", true);
			     INFO(body);
			     REQUIRE_FALSE(p.empty());
			     CHECK(Mentions(p, expect));
		     };
	bad(R"("BinaryControls": [{"Name": "DEV_CMD_01"}])", "\"Index\" is required");
	bad(R"("BinaryControls": [{"Index": 0}])", "\"Name\" is required");
	bad(R"("BinaryControls": [{"Index": 0, "Name": "A"}, {"Index": 0, "Name": "B"}])", "duplicate Index 0 among the \"Serve\" BinaryControls");
	bad(R"("BinaryControls": [{"Index": 0, "Name": "A"}, {"Index": 1, "Name": "A"}])", "duplicate control Name \"A\"");
	bad(R"("BinaryControls": [{"Index": 0, "Name": "A", "Type": "RealSetpoint"}])", "does not suit BinaryControls (Command or DiscreteCommand)");
	bad(R"("AnalogControls": [{"Index": 0, "Name": "A", "Type": "Command"}])", "does not suit AnalogControls (RealSetpoint or DiscreteSetpoint)");
	bad(R"("AnalogControls": [{"Index": 0, "Name": "A", "Type": "Bogus"}])", "unknown control \"Type\" \"Bogus\"");
	bad(R"("BinaryControls": [{"Index": 0, "Name": "A", "CommandValueOn": 1, "CommandValueOff": 1}])", "must differ");
	bad(R"("BinaryControls": [{"Index": 0, "Name": "A", "ControlCodeOn": "NUL"}])", "\"ControlCodeOn\" must be an ODC control code");
	bad(R"("BinaryControls": [{"Index": 0, "Name": "A", "ControlCodeOff": "WRONG"}])", "\"ControlCodeOff\" must be an ODC control code");
	bad(R"("AnalogControls": [{"Index": 0, "Name": "A", "AnalogOutputType": "Int8"}])", "\"AnalogOutputType\" must be");
	bad(R"("AnalogControls": [{"Index": 0, "Name": "A", "ScaleFactor": 0}])", "\"ScaleFactor\" must be a non-zero finite number");
	bad(R"("BinaryControls": [{"Index": 0, "Name": "A", "SelectTimeoutms": 0}])", "\"SelectTimeoutms\" must be between");
	bad(R"("BinaryControls": [{"Index": 0, "Name": "A", "Timeoutms": "x"}])", "\"Timeoutms\" must be an integer");
	bad(R"("BinaryControls": [{"Index": 0, "Name": "A", "SelectRequired": 1}])", "\"SelectRequired\" must be true or false");
	bad(R"("BinaryControls": [{"Index": 0, "Name": "A", "Scope": "XCC"}])", "\"Scope\" must be \"ICC\" or \"VCC\"");
	bad(R"("BinaryControls": {"Index": 0})", "\"BinaryControls\" must be an array");
	bad(R"("AnalogControls": [1])", "AnalogControls[0]: must be an object");
	bad(R"("BinaryControls": [{"Index": 0, "Name": ")" + std::string(254, 'x') + R"("}])", "\"Name\" is required (1 to 251 characters");

	//the device and its select object are two names in one domain
	bad(R"("BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01"}, {"Index": 1, "Name": "DEV_CMD_01_SBO"}])", "clashes with the select object of control \"DEV_CMD_01\"");
	//a control and a point of the same side cannot share a name; on the other side they can
	bad(R"("Binaries": [{"Index": 0, "Name": "DEV_CMD_01", "Direction": "Serve"}], "BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01"}])",
		"has the same object name as a point");

	//the domain
	{
		const auto p = LoadProblems(R"({"ServiceRole": "Server", "BinaryControls": [{"Index": 0, "Name": "A"}]})", true);
		CHECK(Mentions(p, "an ICC control needs a domain: set \"Domain\" or \"LocalDomain\""));
		CHECK(Mentions(p, "\"LocalDomain\" is required when ICC control devices are served"));
		const auto q = LoadProblems(R"({"ServiceRole": "Client", "BinaryControls": [{"Index": 0, "Name": "A"}]})", false);
		CHECK(Mentions(q, "an ICC control needs a domain: set \"Domain\" or \"RemoteDomain\""));
		CHECK(Mentions(q, "\"RemoteDomain\" is required when the peer's devices are operated"));
		const auto ok = LoadProblems(R"({"ServiceRole": "Client", "BinaryControls": [{"Index": 0, "Name": "A", "Scope": "VCC"}]})", false);
		CHECK(ok.empty());
	}
	//the VCC and ICC names and an own Domain
	{
		ICCPPortConf c;
		REQUIRE(LoadProblems(R"({"ServiceRole": "Server", "LocalDomain": "DOMAIN_A", "BinaryControls": [{"Index": 0, "Name": "A", "Domain": "DOMAIN_X"}]})", true, &c).empty());
		CHECK(c.controls[0].domain == "DOMAIN_X");
	}
}

TEST_CASE(SUITE("configuration: CheckBackID"))
{
	ICCPPortConf c;
	REQUIRE(LoadProblems(R"({"ServiceRole": "Peer", "LocalDomain": "DOMAIN_A", "RemoteDomain": "DOMAIN_B",
		"BinaryControls": [{"Index": 0, "Name": "DEV_CMD_01", "Direction": "Serve", "CheckBackID": 40000},
			{"Index": 1, "Name": "DEV_CMD_02", "Direction": "Serve", "CheckBackID": 0},
			{"Index": 0, "Name": "DEV_CMD_01", "Direction": "Operate", "CheckBackID": -32768},
			{"Index": 1, "Name": "DEV_CMD_02", "Direction": "Operate"}],
		"AnalogControls": [{"Index": 0, "Name": "DEV_SP_01", "Direction": "Serve", "CheckBackID": 65535}]})", false, &c).empty());
	REQUIRE(c.controls.size() == 5);
	CHECK(c.controls[0].checkBackId == std::optional<int64_t>(40000));
	CHECK(c.controls[0].ServedCheckBackId() == 40000);
	CHECK(c.controls[1].checkBackId == std::optional<int64_t>(0)); //zero is a value, not absent
	CHECK(c.controls[1].ServedCheckBackId() == 0);
	CHECK(c.controls[2].checkBackId == std::optional<int64_t>(-32768));
	CHECK_FALSE(c.controls[3].checkBackId.has_value());
	CHECK(c.controls[4].checkBackId == std::optional<int64_t>(65535));

	const std::string head = R"({"ServiceRole": "Server", "LocalDomain": "DOMAIN_A", "BinaryControls": [{"Index": 0, "Name": "A", "CheckBackID": )";
	for(const char* ok : {"-32768", "-1", "0", "1", "32767", "32768", "65535"})
	{
		INFO(ok);
		CHECK(LoadProblems(head + ok + "}]}", true).empty());
	}
	for(const char* bad : {"-32769", "65536", "1000000"})
	{
		const auto p = LoadProblems(head + bad + "}]}", true);
		INFO(bad);
		REQUIRE_FALSE(p.empty());
		CHECK(Mentions(p, "BinaryControls[0]: \"CheckBackID\" must be between -32768 and 65535"));
	}
	CHECK(Mentions(LoadProblems(head + R"("x"}]})", true), "BinaryControls[0]: \"CheckBackID\" must be an integer"));
	CHECK(Mentions(LoadProblems(head + R"(1.5}]})", true), "\"CheckBackID\" must be an integer"));
	CHECK(Mentions(LoadProblems(head + R"(true}]})", true), "\"CheckBackID\" must be an integer"));
}

TEST_CASE(SUITE("configuration: the blocks the controls need"))
{
	//serving devices with an explicit SupportedBlocks that lacks 5
	auto p = LoadProblems(R"({"ServiceRole": "Server", "LocalDomain": "D", "SupportedBlocks": [1, 2], "BinaryControls": [{"Index": 0, "Name": "A"}]})", true);
	REQUIRE(p.size() == 1);
	CHECK(Mentions(p, "\"SupportedBlocks\" must contain 5 (device control)"));
	ICCPPortConf c;
	CHECK(LoadProblems(R"({"ServiceRole": "Server", "LocalDomain": "D", "SupportedBlocks": [1, 5], "BinaryControls": [{"Index": 0, "Name": "A"}]})", true, &c).empty());
	CHECK(c.supportedBlocks == std::set<int>({1, 5})); //what was given is what is advertised
	//operating the peer's with RequiredRemoteBlocks that lacks 5
	p = LoadProblems(R"({"ServiceRole": "Client", "RemoteDomain": "D", "RequiredRemoteBlocks": [1], "BinaryControls": [{"Index": 0, "Name": "A"}]})", false);
	REQUIRE(p.size() == 1);
	CHECK(Mentions(p, "\"RequiredRemoteBlocks\" must contain 5 (device control)"));
	CHECK(LoadProblems(R"({"ServiceRole": "Client", "RemoteDomain": "D", "RequiredRemoteBlocks": [1, 5], "BinaryControls": [{"Index": 0, "Name": "A"}]})", false, &c).empty());
	CHECK(c.requiredRemoteBlocks == std::set<int>({1, 5}));
	//no controls, no change
	CHECK(LoadProblems(R"({"ServiceRole": "Peer"})", false, &c).empty());
	CHECK(c.supportedBlocks == std::set<int>({1, 2}));
	CHECK(c.requiredRemoteBlocks == std::set<int>({1}));
	//loading again resets what the earlier load added
	Json::Value root;
	{
		Json::CharReaderBuilder builder;
		std::string errs;
		std::istringstream in(R"({"ServiceRole": "Server", "LocalDomain": "D", "BinaryControls": [{"Index": 0, "Name": "A"}]})");
		REQUIRE(Json::parseFromStream(builder, in, &root, &errs));
	}
	ICCPPortConf twice;
	REQUIRE(twice.Load(root, true).empty());
	CHECK(twice.supportedBlocks.count(5) == 1);
	Json::Value none(Json::objectValue);
	none["ServiceRole"] = "Server";
	REQUIRE(twice.Load(none, true).empty());
	CHECK(twice.supportedBlocks == std::set<int>({1, 2}));
	CHECK(twice.controls.empty());
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the width of the values: an integer of width 16 (IEC 60870-6-802 clause 6.2.2)
// ---------------------------------------------------------------------------------------------------------------------------------

TEST_CASE(SUITE("width 16, server side: an operate whose integer does not fit is refused with object-attribute-inconsistent; -32768 to 65535 is accepted"))
{
	B5Fixture f;
	f.StartClient();
	//DEV_SP_04: a discrete setpoint that needs no select (scale 1, ODC type Int32)
	for(int64_t v : {int64_t(-32768), int64_t(-1), int64_t(0), int64_t(32767), int64_t(32768), int64_t(65535)})
	{
		INFO(v);
		CHECK(f.OperateRaw("DEV_SP_04", ControlType::DiscreteSetpoint, v) == std::nullopt);
	}
	REQUIRE(f.ops.size() == 6);
	CHECK(f.ops[0].integer == -32768);
	CHECK(f.ops[5].integer == 65535);
	CHECK(f.ops[5].value == Approx(65535.0));
	//unsigned encodings of the same range
	CHECK(f.WriteBare("DEV_SP_04", mms::Data::Unsigned(65535)) == std::nullopt);
	CHECK(f.WriteBare("DEV_SP_04", mms::Data::Unsigned(0)) == std::nullopt);
	CHECK(f.ops.size() == 8);
	CHECK(f.server->ControlCounters().refused == 0);

	f.ops.clear();
	for(int64_t v : {int64_t(-32769), int64_t(65536), int64_t(100000), int64_t(2147483648LL), INT64_MIN, INT64_MAX})
	{
		INFO(v);
		CHECK(f.OperateRaw("DEV_SP_04", ControlType::DiscreteSetpoint, v) == std::optional<int64_t>(DA::ObjectAttributeInconsistent));
	}
	CHECK(f.WriteBare("DEV_SP_04", mms::Data::Unsigned(65536)) == std::optional<int64_t>(DA::ObjectAttributeInconsistent));
	CHECK(f.WriteBare("DEV_SP_04", mms::Data::Unsigned(UINT64_MAX)) == std::optional<int64_t>(DA::ObjectAttributeInconsistent));
	CHECK(f.ops.empty()); //nothing went downstream
	CHECK(f.server->ControlCounters().refused == 8);
	CHECK(f.Count("control DEV_SP_04: operate refused: out of range: the value 65536 does not fit an integer of width 16 (accepted: -32768 to 65535) "
		"(data access error object-attribute-inconsistent [8])", LogLevel::Warn) == 2);
	CHECK(f.Count("control DEV_SP_04: operate refused: out of range: the value -32769 does not fit", LogLevel::Warn) == 1);
	CHECK(f.Count("control DEV_SP_04: operate refused: out of range: the value 18446744073709551615 does not fit", LogLevel::Warn) == 1);

	//the same for a command device: the width is checked before the ON / OFF values (DEV_CMD_02: ON 2, OFF 3)
	CHECK(f.OperateRaw("DEV_CMD_02", ControlType::Command, 70000) == std::optional<int64_t>(DA::ObjectAttributeInconsistent));
	CHECK(f.OperateRaw("DEV_CMD_02", ControlType::Command, -32769) == std::optional<int64_t>(DA::ObjectAttributeInconsistent));
	CHECK(f.OperateRaw("DEV_CMD_02", ControlType::Command, 4) == std::optional<int64_t>(DA::TypeUnsupported)); //in range, but neither ON nor OFF
	CHECK(f.OperateRaw("DEV_CMD_02", ControlType::Command, 2) == std::nullopt);
	CHECK(f.ops.size() == 1);

	//a real setpoint is a 32 bit float: no integer width applies
	CHECK(f.OperateRaw("DEV_SP_03", ControlType::RealSetpoint, 0, 1e10) == std::nullopt);
}

TEST_CASE(SUITE("width 16, server side: a refused operate leaves a selected device selected, and the downstream conversion still applies after the width check"))
{
	B5Fixture f;
	f.StartClient();
	//DEV_SP_02: a discrete setpoint that needs a select, published downstream as Int16
	CHECK(f.ReadSbo("DEV_SP_02").second == std::nullopt);
	CHECK(f.OperateRaw("DEV_SP_02", ControlType::DiscreteSetpoint, 65536) == std::optional<int64_t>(DA::ObjectAttributeInconsistent));
	CHECK(f.OperateRaw("DEV_SP_02", ControlType::DiscreteSetpoint, 7) == std::nullopt);
	REQUIRE(f.ops.size() == 1);
	CHECK(f.ops[0].integer == 7);
	CHECK(f.ops[0].control->analogEvent == odc::EventType::AnalogOutputInt16);
	//40000 fits the width of the type (as an unsigned value), so it is the downstream conversion to Int16 (ICCPPortBase::PublishControl) that has the say about it:
	//the session passes it on, scaled, once
	CHECK(f.ReadSbo("DEV_SP_02").second == std::nullopt);
	CHECK(f.OperateRaw("DEV_SP_02", ControlType::DiscreteSetpoint, 40000) == std::nullopt);
	REQUIRE(f.ops.size() == 2);
	CHECK(f.ops[1].value == Approx(40000.0));
	CHECK(f.Count("out of range", LogLevel::Warn) == 1); //only the first one is a refusal of the session
}

TEST_CASE(SUITE("width 16, client side: a command or a discrete setpoint outside -32768 to 32767 is refused before anything is sent"))
{
	B5Fixture f;
	f.StartClient();
	//DEV_SP_04: direct, scale 1; the value is rounded first
	struct Case
	{
		double value;
		bool ok;
		int64_t wire;
	};
	const Case cases[] = {
		{-32768, true, -32768}, {32767, true, 32767}, {0, true, 0}, {32767.4, true, 32767}, {-32768.4, true, -32768},
		{32767.5, false, 0}, {-32768.5, false, 0}, {32768, false, 0}, {-32769, false, 0}, {65535, false, 0}, {65536, false, 0}, {1e12, false, 0}, {-1e12, false, 0}
	};
	size_t failures = 0;
	for(const auto& c : cases)
	{
		INFO(c.value);
		f.wire.clear();
		const auto r = f.Operate("DEV_SP_04", B5Fixture::Value(c.value));
		REQUIRE(r->has_value());
		if(c.ok)
		{
			CHECK(**r == CS::SUCCESS);
			CHECK(f.wire == std::vector<std::string>({"Write DEV_SP_04"}));
			CHECK(f.wireData.back() == mms::Data::Integer(c.wire));
		}
		else
		{
			CHECK(**r == CS::OUT_OF_RANGE);
			CHECK(f.wire.empty());
			++failures;
		}
	}
	CHECK(f.client->ControlCounters().failures == failures);
	CHECK(f.Count("control DEV_SP_04 not started: the setpoint 65536.000000 is 65536.000000 on the wire, which does not fit an integer of width 16 (-32768 to 32767) (OUT_OF_RANGE)", LogLevel::Error) == 1);
	CHECK(f.Count("does not fit an integer of width 16", LogLevel::Error) == failures);

	//with a select (DEV_SP_02) nothing is read either
	f.wire.clear();
	CHECK(**f.Operate("DEV_SP_02", B5Fixture::Value(40000)) == CS::OUT_OF_RANGE);
	CHECK(f.wire.empty());
	CHECK(**f.Operate("DEV_SP_02", B5Fixture::Value(32767)) == CS::SUCCESS);
	CHECK(f.wire == std::vector<std::string>({"Read DEV_SP_02_SBO", "Write DEV_SP_02"}));

	//the scaling comes first: ODC 100000 with a ScaleFactor of 4 is 25000 on the wire
	f.clientConf.controls[4].scale = 4.0; //DEV_SP_02
	CHECK(**f.Operate("DEV_SP_02", B5Fixture::Value(100000)) == CS::SUCCESS);
	CHECK(f.wireData.back() == mms::Data::Integer(25000));
	CHECK(**f.Operate("DEV_SP_02", B5Fixture::Value(140000)) == CS::OUT_OF_RANGE);

	//a real setpoint is not an integer
	CHECK(**f.Operate("DEV_SP_01", B5Fixture::Value(1e9)) == CS::SUCCESS);

	//a command value outside the width (the configuration cannot say so, see below, but a changed value must not reach the wire)
	f.clientConf.controls[1].commandValueOn = 40000; //DEV_CMD_02: direct
	f.wire.clear();
	CHECK(**f.Operate("DEV_CMD_02", B5Fixture::On()) == CS::OUT_OF_RANGE);
	CHECK(f.wire.empty());
	CHECK(f.Count("control DEV_CMD_02 not started: the command value 40000 does not fit an integer of width 16 (-32768 to 32767) (OUT_OF_RANGE)", LogLevel::Error) == 1);
	CHECK(**f.Operate("DEV_CMD_02", B5Fixture::Off()) == CS::SUCCESS);
}

TEST_CASE(SUITE("width 16, configuration: CommandValueOn and CommandValueOff are -32768 to 32767"))
{
	const std::string head = R"({"ServiceRole": "Server", "LocalDomain": "DOMAIN_A", "BinaryControls": [{"Index": 0, "Name": "A", )";
	auto load = [&](const std::string& on, const std::string& off)
			{
				return LoadProblems(head + R"("CommandValueOn": )" + on + R"(, "CommandValueOff": )" + off + "}]}", true);
			};
	CHECK(load("32767", "-32768").empty());
	CHECK(load("-32768", "32767").empty());
	CHECK(load("1", "0").empty());
	for(const char* bad : {"32768", "-32769", "65535", "65536", "2147483647", "9223372036854775807"})
	{
		INFO(bad);
		auto p = load(bad, "0");
		REQUIRE(p.size() == 1);
		CHECK(Mentions(p, "BinaryControls[0]: \"CommandValueOn\" must be between -32768 and 32767: a command is an integer of width 16"));
		CHECK(Mentions(p, "IEC 60870-6-802 clause 6.2.2"));
		p = load("1", bad);
		REQUIRE(p.size() == 1);
		CHECK(Mentions(p, "\"CommandValueOff\" must be between -32768 and 32767"));
	}
	CHECK(Mentions(load("\"1\"", "0"), "\"CommandValueOn\" must be an integer"));
	//the other rule still holds
	CHECK(Mentions(load("5", "5"), "must differ"));
	//CheckBackID keeps its own range (the width as an unsigned integer too)
	CHECK(LoadProblems(head + R"("CheckBackID": 65535}]})", true).empty());
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Bilateral_Table_ID: a visible string of at most 32 characters (IEC 60870-6-503 clause 8.3.3)
// ---------------------------------------------------------------------------------------------------------------------------------

TEST_CASE(SUITE("Bilateral_Table_ID: the type advertised is a visible string of at most 32 characters"))
{
	B5Fixture f;
	f.StartClient();
	const auto o = f.Request(tase2::BuildGetDataValueType(B5Fixture::Dev("Bilateral_Table_ID")));
	const auto* r = o.Get<mms::GetVariableAccessAttributesResponse>();
	REQUIRE(r != nullptr);
	CHECK_FALSE(r->mmsDeletable);
	CHECK(r->typeDescription.kind == mms::TypeDescription::Kind::VisibleString);
	CHECK(r->typeDescription.size == -32); //a varying length, 32 at most
	CHECK(r->typeDescription == tase2::BilateralTableIdTypeDescription());
	//the value we serve fits it, and the type is only for the domain that has the object
	const auto v = f.Request(tase2::BuildGetDataValue({B5Fixture::Dev("Bilateral_Table_ID")}));
	const auto* rr = v.Get<mms::ReadResponse>();
	REQUIRE(rr != nullptr);
	REQUIRE(rr->results.size() == 1);
	CHECK(rr->results[0].data == mms::Data::VisibleString("BT_TEST_A"));
	const auto other = f.Request(tase2::BuildGetDataValueType(mms::ObjectName::Domain("DOMAIN_X", "Bilateral_Table_ID")));
	CHECK(other.Get<mms::GetVariableAccessAttributesResponse>() == nullptr);
	CHECK(tase2::BilateralTableIdWidth == 32u);
}

TEST_CASE(SUITE("Bilateral_Table_ID: a peer's longer string is warned about once and still compared as before"))
{
	const std::string id33(33, 'y');
	{
		//the client expects exactly what the peer sends: no problem, one warning
		B5Fixture f;
		f.serverConf.bilateralTableId = id33;
		f.clientConf.remoteBilateralTableId = id33;
		f.StartClient();
		CHECK(f.client->Ready());
		CHECK(f.fatal.empty());
		CHECK(f.link.a.conn.IsAssociated());
		CHECK(f.Count("the peer's Bilateral_Table_ID is 33 characters long, more than the 32 of its type Visible-String-32", LogLevel::Warn) == 1);
	}
	{
		//a mismatch is the same mismatch as ever
		B5Fixture f;
		f.serverConf.bilateralTableId = id33;
		f.clientConf.remoteBilateralTableId = "BT_TEST_A";
		f.StartClient();
		CHECK_FALSE(f.client->Ready());
		CHECK(f.fatal.find("Bilateral_Table_ID mismatch") != std::string::npos);
		CHECK(f.Count("the peer's Bilateral_Table_ID is 33 characters long", LogLevel::Warn) == 1);
	}
	{
		//32 characters: nothing to say
		B5Fixture f;
		f.serverConf.bilateralTableId = std::string(32, 'y');
		f.StartClient();
		CHECK(f.client->Ready());
		CHECK(f.Count("characters long", LogLevel::Warn) == 0);
	}
}
