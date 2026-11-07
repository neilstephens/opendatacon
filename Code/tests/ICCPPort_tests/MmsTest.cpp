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
 * MmsTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Unit tests for the MMS (ISO 9506) codec.
 *
 *  The fixtures are synthetic: they were assembled with an independent encoder from invented names and
 *  values, but keep the byte structure (tags, wrappers, optional fields, length forms) of real ICCP
 *  traffic. Large messages (name pages, data set definitions, reports) are generated programmatically.
 */

#include "Mms.h"
#include <algorithm>
#include <catch.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#define SUITE(name) "ICCP MMS - " name

namespace
{
using namespace iccp::mms;

Bytes Hex(const std::string& s)
{
	Bytes out;
	int hi = -1;
	for(char c : s)
	{
		int v;
		if(c >= '0' && c <= '9') v = c - '0';
		else if(c >= 'a' && c <= 'f') v = c - 'a' + 10;
		else if(c >= 'A' && c <= 'F') v = c - 'A' + 10;
		else continue;
		if(hi < 0) hi = v;
		else
		{
			out.push_back(static_cast<uint8_t>(hi * 16 + v));
			hi = -1;
		}
	}
	return out;
}

//BER length octets
Bytes Len(size_t n)
{
	if(n < 128)
		return Bytes{static_cast<uint8_t>(n)}
	;
	Bytes digits;
	for(size_t v = n; v != 0; v >>= 8)
		digits.insert(digits.begin(), static_cast<uint8_t>(v & 0xFF));
	digits.insert(digits.begin(), static_cast<uint8_t>(0x80 | digits.size()));
	return digits;
}

Bytes Tlv(uint8_t tag, const Bytes& content)
{
	Bytes out{tag};
	const Bytes len = Len(content.size());
	out.insert(out.end(), len.begin(), len.end());
	out.insert(out.end(), content.begin(), content.end());
	return out;
}

Bytes Cat(std::initializer_list<Bytes> parts)
{
	Bytes out;
	for(const auto& p : parts)
		out.insert(out.end(), p.begin(), p.end());
	return out;
}

//a name such as POINT_0042_MW
std::string PointName(size_t i, const char* suffix = "MW")
{
	char buf[32];
	snprintf(buf, sizeof(buf), "POINT_%04zu_%s", i, suffix);
	return buf;
}

// ---- fixtures (synthetic) ----
const char* GnlReqDomainNvl = "a01702021234a111a003800102a10a8108444f4d41494e5f41";
const char* GnlReqVmd = "a00f02021234a109a003800100a1028000";
const char* GnlReqVmdCont = "a01e02021234a118a003800100a1028000820d504f494e545f303130305f4d57";
const char* GnlReqAa = "a00f02021234a109a003800109a1028200";
const char* GnlRespLast = "a13802021234a132a02d1a0d504f494e545f303030315f4d571a0d504f494e545f303030325f4b561a0d504f494e545f303030335f4342810100";
const char* GnlRespDefault = "a13502021234a12fa02d1a0d504f494e545f303030315f4d571a0d504f494e545f303030325f4b561a0d504f494e545f303030335f4342";
const char* GnlRespTrue = "a13802021234a132a02d1a0d504f494e545f303030315f4d571a0d504f494e545f303030325f4b561a0d504f494e545f303030335f4342810101";
const char* IdentReq = "a006020212348200";
const char* IdentResp = "a12002021234a21a800856454e444f525f5881074d4f44454c5f5982055245565f5a";
const char* IdentRespSyntaxes = "a12902021234a223800856454e444f525f5881074d4f44454c5f5982055245565f5aa307060528ca220201";
const char* ReadReqList = "a04f02021234a449a147a045301da01ba1191a08444f4d41494e5f411a0d504f494e545f303030315f4d573011a00f800d504f494e545f303030325f4b563011a00f820d504f494e545f303030335f4342";
const char* ReadReqSwr = "a05202021234a44c800101a147a045301da01ba1191a08444f4d41494e5f411a0d504f494e545f303030315f4d573011a00f800d504f494e545f303030325f4b563011a00f820d504f494e545f303030335f4342";
const char* ReadReqListName = "a02302021234a41da11ba119a1171a08444f4d41494e5f411a0b44535f544553545f303031";
const char* ReadResp = "a12902021234a423a1218a0756414c55455f31840304a000a207850207d185010780010a830101860200c8";
const char* WriteReq12 = "a06702021234a561a01c301aa018a1161a08444f4d41494e5f411a0a54535f544553545f3031a041a23fa21a8601018a08444f4d41494e5f418a0b44535f544553545f30303185010085010585010085010085020258840203c0830100830100830101830101850100";
const char* WriteRespOk = "a10802021234a5028100";
const char* WriteRespMixed = "a10b02021234a5058100800103";
const char* DefNvlReq = "a05302021234ab4da1171a08444f4d41494e5f411a0b44535f544553545f303031a032301da01ba1191a08444f4d41494e5f411a0d504f494e545f303030315f4d573011a00f800d504f494e545f303030325f4b56";
const char* DefNvlResp = "a106020212348b00";
const char* DelNvlReq = "a02102021234ad1ba119a1171a08444f4d41494e5f411a0b44535f544553545f303031";
const char* DelNvlReqScope = "a01302021234ad0d8001028208444f4d41494e5f41";
const char* DelNvlResp = "a10c02021234ad06800101810101";
const char* GetNvlAttrReq = "a01f02021234ac19a1171a08444f4d41494e5f411a0b44535f544553545f303031";
const char* GetNvlAttrResp = "a13d02021234ac37800101a132301da01ba1191a08444f4d41494e5f411a0d504f494e545f303030315f4d573011a00f800d504f494e545f303030325f4b56";
const char* GvaaReq = "a02302021234a61da01ba1191a08444f4d41494e5f411a0d504f494e545f303030315f4d57";
const char* GvaaResp = "a12902021234a623800100a21ea21ca11a300980024944a103850120300d800156a108a706020120020108";
const char* InfoReport = "a34ea04ca119a1171a08444f4d41494e5f411a0b44535f544553545f303031a02fa2198601018a08444f4d41494e5f418a0a54535f544553545f3031a20b87050842f6e9798402000085010084020080";
const char* ConfErr = "a20b80021234a205a003820101";
const char* ConfErrExtra = "a21780021234810102a20ea00387010381010782046f6f7073";
const char* RejectPdu = "a40780021234810100";
const char* RejectNoId = "a403850101";
const char* ConcludeReqPdu = "8b00";
const char* ConcludeRespPdu = "8c00";
const char* ConcludeErrPdu = "ad05a003890100";
const char* InitErrPdu = "aa05a003880101";
const char* InitReqPdu = "a82580024e2081010a82010a830103a416800101810305f800820c035c1c000000000004000190";
const char* InitRespPdu = "a92580024e20810104820104830103a416800101810305e000820c035c18000000000000000110";

const std::vector<const char*> AllFixtures = {
	GnlReqDomainNvl, GnlReqVmd, GnlReqVmdCont, GnlReqAa, GnlRespLast, GnlRespDefault, GnlRespTrue, IdentReq, IdentResp,
	IdentRespSyntaxes, ReadReqList, ReadReqSwr, ReadReqListName, ReadResp, WriteReq12, WriteRespOk, WriteRespMixed,
	DefNvlReq, DefNvlResp, DelNvlReq, DelNvlReqScope, DelNvlResp, GetNvlAttrReq, GetNvlAttrResp, GvaaReq, GvaaResp,
	InfoReport, ConfErr, ConfErrExtra, RejectPdu, RejectNoId, ConcludeReqPdu, ConcludeRespPdu, ConcludeErrPdu,
	InitErrPdu, InitReqPdu, InitRespPdu};

template<class T>
T Decoded(const char* hex)
{
	return std::get<T>(Decode(Hex(hex)));
}

template<class S>
S Service(const ConfirmedRequest& r)
{
	return std::get<S>(r.service);
}
template<class S>
S Service(const ConfirmedResponse& r)
{
	return std::get<S>(r.service);
}

//decode, check the byte-identical re-encode, return the PDU
Pdu RoundTrip(const char* hex)
{
	const Bytes wire = Hex(hex);
	const Pdu p = Decode(wire);
	REQUIRE(Encode(p) == wire);
	return p;
}

void RequireEncodes(const Pdu& p, const char* hex)
{
	REQUIRE(Encode(p) == Hex(hex));
}

VariableAccessSpecification ThreeVariables()
{
	return ListOfVariable{VariableSpec{ObjectName::Domain("DOMAIN_A", "POINT_0001_MW"), std::nullopt},
	                      VariableSpec{ObjectName::Vmd("POINT_0002_KV"), std::nullopt},
	                      VariableSpec{ObjectName::Aa("POINT_0003_CB"), std::nullopt}
	};
}
}

TEST_CASE(SUITE("Initiate request and response"))
{
	const InitiateRequest req = Decoded<InitiateRequest>(InitReqPdu);
	REQUIRE(req.localDetailCalling == 20000);
	REQUIRE(req.proposedMaxServOutstandingCalling == 10);
	REQUIRE(req.proposedMaxServOutstandingCalled == 10);
	REQUIRE(req.proposedDataStructureNestingLevel == 3);
	REQUIRE(req.proposedVersionNumber == 1);
	REQUIRE(req.proposedParameterCbb == MakeBits({ParameterBit::Str1, ParameterBit::Str2, ParameterBit::Vnam, ParameterBit::Valt, ParameterBit::Vadr}, 11));
	REQUIRE(req.servicesSupportedCalling.BitCount() == 85);
	for(size_t bit : {ServiceBit::GetNameList, ServiceBit::Rename, ServiceBit::Read, ServiceBit::Write,
	                  ServiceBit::DefineNamedVariableList, ServiceBit::GetNamedVariableListAttributes, ServiceBit::DeleteNamedVariableList,
	                  size_t(61), ServiceBit::InformationReport, size_t(80), ServiceBit::Conclude})
		REQUIRE(req.servicesSupportedCalling.Get(bit));
	REQUIRE_FALSE(req.servicesSupportedCalling.Get(ServiceBit::Identify));
	REQUIRE_FALSE(req.servicesSupportedCalling.Get(ServiceBit::Cancel));
	REQUIRE_FALSE(req.additionalSupportedCalling.has_value());
	RoundTrip(InitReqPdu);

	//from scratch
	InitiateRequest b;
	b.localDetailCalling = 20000;
	b.proposedMaxServOutstandingCalling = 10;
	b.proposedMaxServOutstandingCalled = 10;
	b.proposedDataStructureNestingLevel = 3;
	b.proposedVersionNumber = 1;
	b.proposedParameterCbb = MakeBits({0, 1, 2, 3, 4}, 11);
	b.servicesSupportedCalling = MakeBits({1, 3, 4, 5, 11, 12, 13, 61, 79, 80, 83}, 85);
	RequireEncodes(b, InitReqPdu);
	REQUIRE(b == req);

	const InitiateResponse resp = Decoded<InitiateResponse>(InitRespPdu);
	REQUIRE(resp.localDetailCalled == 20000);
	REQUIRE(resp.negotiatedMaxServOutstandingCalling == 4);
	REQUIRE(resp.negotiatedMaxServOutstandingCalled == 4);
	REQUIRE(resp.negotiatedDataStructureNestingLevel == 3);
	REQUIRE(resp.negotiatedParameterCbb == MakeBits({0, 1, 2}, 11));
	REQUIRE(resp.servicesSupportedCalled == MakeBits({1, 3, 4, 5, 11, 12, 79, 83}, 85));
	RoundTrip(InitRespPdu);

	InitiateResponse r;
	r.localDetailCalled = 20000;
	r.negotiatedMaxServOutstandingCalling = 4;
	r.negotiatedMaxServOutstandingCalled = 4;
	r.negotiatedDataStructureNestingLevel = 3;
	r.negotiatedParameterCbb = MakeBits({0, 1, 2}, 11);
	r.servicesSupportedCalled = MakeBits({1, 3, 4, 5, 11, 12, 79, 83}, 85);
	RequireEncodes(r, InitRespPdu);

	//optional fields: all present
	InitiateRequest full = b;
	full.additionalSupportedCalling = MakeBits({0}, 3);
	full.additionalCbbSupportedCalling = MakeBits({1}, 5);
	full.privilegeClassIdentityCalling = "CLASS_X";
	const Bytes wire = Encode(full);
	REQUIRE(std::get<InitiateRequest>(Decode(wire)) == full);
	//and none of the optional ones
	InitiateRequest bare;
	bare.proposedParameterCbb = MakeBits({0}, 1);
	bare.servicesSupportedCalling = MakeBits({1}, 2);
	REQUIRE(std::get<InitiateRequest>(Decode(Encode(bare))) == bare);
}

TEST_CASE(SUITE("Initiate error, conclude and reject PDUs"))
{
	const Pdu ce = RoundTrip(ConcludeErrPdu);
	REQUIRE(std::get<ConcludeError>(ce).error.errorClass == ErrorClass::Conclude);
	REQUIRE(std::get<ConcludeError>(ce).error.errorCode == 0);
	const Pdu ie = RoundTrip(InitErrPdu);
	REQUIRE(std::get<InitiateError>(ie).error.errorClass == ErrorClass::Initiate);
	REQUIRE(std::get<InitiateError>(ie).error.errorCode == 1);

	REQUIRE(std::holds_alternative<ConcludeRequest>(RoundTrip(ConcludeReqPdu)));
	REQUIRE(std::holds_alternative<ConcludeResponse>(RoundTrip(ConcludeRespPdu)));
	RequireEncodes(ConcludeRequest{}, ConcludeReqPdu);
	RequireEncodes(ConcludeResponse{}, ConcludeRespPdu);

	const Reject rj = Decoded<Reject>(RejectPdu);
	REQUIRE(rj.originalInvokeId == 0x1234u);
	REQUIRE(rj.type == RejectType::ConfirmedRequestPdu);
	REQUIRE(rj.reason == 0);
	RoundTrip(RejectPdu);
	RequireEncodes(rj, RejectPdu);

	const Reject nid = Decoded<Reject>(RejectNoId);
	REQUIRE_FALSE(nid.originalInvokeId.has_value());
	REQUIRE(nid.type == RejectType::PduError);
	REQUIRE(nid.reason == 1);
	RoundTrip(RejectNoId);

	ConcludeError c;
	c.error = ServiceError{ErrorClass::Conclude, 0, std::nullopt, std::nullopt, std::nullopt};
	RequireEncodes(c, ConcludeErrPdu);
	InitiateError i;
	i.error = ServiceError{ErrorClass::Initiate, 1, std::nullopt, std::nullopt, std::nullopt};
	RequireEncodes(i, InitErrPdu);
}

TEST_CASE(SUITE("Confirmed error"))
{
	const ConfirmedError e = Decoded<ConfirmedError>(ConfErr);
	REQUIRE(e.invokeId == 0x1234);
	REQUIRE_FALSE(e.modifierPosition.has_value());
	REQUIRE(e.error.errorClass == ErrorClass::Definition);
	REQUIRE(e.error.errorCode == 1);
	REQUIRE_FALSE(e.error.additionalCode.has_value());
	RoundTrip(ConfErr);
	RequireEncodes(ConfirmedError{0x1234, std::nullopt, ServiceError{ErrorClass::Definition, 1, {}, {}, {}
						}
		}, ConfErr);

	const ConfirmedError x = Decoded<ConfirmedError>(ConfErrExtra);
	REQUIRE(x.modifierPosition == 2);
	REQUIRE(x.error.errorClass == ErrorClass::Access);
	REQUIRE(x.error.errorCode == 3);
	REQUIRE(x.error.additionalCode == 7);
	REQUIRE(x.error.additionalDescription == "oops");
	RoundTrip(ConfErrExtra);

	//service specific information is carried opaquely
	ServiceError s;
	s.errorClass = ErrorClass::Service;
	s.serviceSpecificInformation = Hex("a3 03 80 01 05");
	const Bytes wire = Encode(ConfirmedError{9, std::nullopt, s});
	const ConfirmedError back = std::get<ConfirmedError>(Decode(wire));
	REQUIRE(back.error.serviceSpecificInformation == s.serviceSpecificInformation);
	REQUIRE(Encode(back) == wire);
}

TEST_CASE(SUITE("Identify"))
{
	const ConfirmedRequest req = Decoded<ConfirmedRequest>(IdentReq);
	REQUIRE(req.invokeId == 0x1234);
	REQUIRE(std::holds_alternative<IdentifyRequest>(req.service));
	RoundTrip(IdentReq);
	RequireEncodes(ConfirmedRequest{0x1234, IdentifyRequest{}
		}, IdentReq);

	const ConfirmedResponse resp = Decoded<ConfirmedResponse>(IdentResp);
	const IdentifyResponse id = Service<IdentifyResponse>(resp);
	REQUIRE(id.vendorName == "VENDOR_X");
	REQUIRE(id.modelName == "MODEL_Y");
	REQUIRE(id.revision == "REV_Z");
	REQUIRE_FALSE(id.listOfAbstractSyntaxes.has_value());
	RoundTrip(IdentResp);
	RequireEncodes(ConfirmedResponse{0x1234, IdentifyResponse{"VENDOR_X", "MODEL_Y", "REV_Z", std::nullopt}
		}, IdentResp);

	const IdentifyResponse withSyntaxes = Service<IdentifyResponse>(Decoded<ConfirmedResponse>(IdentRespSyntaxes));
	REQUIRE(withSyntaxes.listOfAbstractSyntaxes->size() == 1);
	REQUIRE((*withSyntaxes.listOfAbstractSyntaxes)[0] == Oid({1, 0, 9506, 2, 1}));
	RoundTrip(IdentRespSyntaxes);
}

TEST_CASE(SUITE("GetNameList"))
{
	GetNameListRequest r = Service<GetNameListRequest>(Decoded<ConfirmedRequest>(GnlReqDomainNvl));
	REQUIRE(r.objectClass == ObjectClass::NamedVariableList);
	REQUIRE(r.scope == ObjectScope::DomainSpecific);
	REQUIRE(r.domainId == "DOMAIN_A");
	REQUIRE_FALSE(r.continueAfter.has_value());
	RoundTrip(GnlReqDomainNvl);
	RequireEncodes(ConfirmedRequest{0x1234, r}, GnlReqDomainNvl);

	r = Service<GetNameListRequest>(Decoded<ConfirmedRequest>(GnlReqVmd));
	REQUIRE(r.objectClass == ObjectClass::NamedVariable);
	REQUIRE(r.scope == ObjectScope::VmdSpecific);
	RoundTrip(GnlReqVmd);
	RequireEncodes(ConfirmedRequest{0x1234, r}, GnlReqVmd);

	r = Service<GetNameListRequest>(Decoded<ConfirmedRequest>(GnlReqVmdCont));
	REQUIRE(r.continueAfter == "POINT_0100_MW");
	RoundTrip(GnlReqVmdCont);
	RequireEncodes(ConfirmedRequest{0x1234, r}, GnlReqVmdCont);

	r = Service<GetNameListRequest>(Decoded<ConfirmedRequest>(GnlReqAa));
	REQUIRE(r.objectClass == ObjectClass::Domain);
	REQUIRE(r.scope == ObjectScope::AaSpecific);
	RoundTrip(GnlReqAa);

	const std::vector<std::string> names = {"POINT_0001_MW", "POINT_0002_KV", "POINT_0003_CB"};
	GetNameListResponse last = Service<GetNameListResponse>(Decoded<ConfirmedResponse>(GnlRespLast));
	REQUIRE(last.identifiers == names);
	REQUIRE(last.moreFollows == false);
	REQUIRE_FALSE(last.MoreFollows());
	RoundTrip(GnlRespLast);
	RequireEncodes(ConfirmedResponse{0x1234, last}, GnlRespLast);

	//moreFollows absent means TRUE; and it is kept as sent
	GetNameListResponse def = Service<GetNameListResponse>(Decoded<ConfirmedResponse>(GnlRespDefault));
	REQUIRE_FALSE(def.moreFollows.has_value());
	REQUIRE(def.MoreFollows());
	RoundTrip(GnlRespDefault);
	RequireEncodes(ConfirmedResponse{0x1234, def}, GnlRespDefault);

	GetNameListResponse tru = Service<GetNameListResponse>(Decoded<ConfirmedResponse>(GnlRespTrue));
	REQUIRE(tru.moreFollows == true);
	REQUIRE(tru.MoreFollows());
	RoundTrip(GnlRespTrue);

	//an empty page
	const Bytes empty = Encode(ConfirmedResponse{1, GetNameListResponse{
									   {}, false}
		});
	REQUIRE(empty == Hex("a1 0a 02 01 01 a1 05 a0 00 81 01 00"));
	REQUIRE(std::get<ConfirmedResponse>(Decode(empty)) == ConfirmedResponse{1, GetNameListResponse{
														{}, false}
		});
}

TEST_CASE(SUITE("GetNameList pages of hundreds of names"))
{
	for(size_t count : {size_t(1), size_t(100), size_t(939), size_t(5000)})
	{
		GetNameListResponse page;
		for(size_t i = 0; i < count; ++i)
			page.identifiers.push_back(PointName(i));
		const Bytes wire = Encode(ConfirmedResponse{0x1234, page});

		//independent size expectation: each identifier is 2 + 13 octets
		const size_t listContent = count * 15;
		const Bytes list = Cat({Bytes{0xa0}, Len(listContent)});
		const size_t svcContent = list.size() + listContent;
		const Bytes svc = Cat({Bytes{0xa1}, Len(svcContent)});
		const size_t pduContent = 4 + svc.size() + svcContent;
		const Bytes pdu = Cat({Bytes{0xa1}, Len(pduContent)});
		REQUIRE(wire.size() == pdu.size() + pduContent);
		REQUIRE(Bytes(wire.begin(), wire.begin() + pdu.size()) == pdu);
		REQUIRE(Bytes(wire.begin() + pdu.size(), wire.begin() + pdu.size() + 4) == Hex("02 02 12 34"));
		REQUIRE(Bytes(wire.begin() + pdu.size() + 4, wire.begin() + pdu.size() + 4 + svc.size()) == svc);

		const ConfirmedResponse back = std::get<ConfirmedResponse>(Decode(wire));
		const GetNameListResponse got = Service<GetNameListResponse>(back);
		REQUIRE(got.identifiers.size() == count);
		REQUIRE(got.identifiers.front() == PointName(0));
		REQUIRE(got.identifiers.back() == PointName(count - 1));
		REQUIRE(got == page);
		REQUIRE(Encode(back) == wire);
	}
}

TEST_CASE(SUITE("Read"))
{
	ReadRequest r = Service<ReadRequest>(Decoded<ConfirmedRequest>(ReadReqList));
	REQUIRE_FALSE(r.specificationWithResult);
	REQUIRE(r.specification == ThreeVariables());
	RoundTrip(ReadReqList);
	RequireEncodes(ConfirmedRequest{0x1234, r}, ReadReqList);

	r = Service<ReadRequest>(Decoded<ConfirmedRequest>(ReadReqSwr));
	REQUIRE(r.specificationWithResult);
	REQUIRE(r.specification == ThreeVariables());
	RoundTrip(ReadReqSwr);
	RequireEncodes(ConfirmedRequest{0x1234, r}, ReadReqSwr);

	r = Service<ReadRequest>(Decoded<ConfirmedRequest>(ReadReqListName));
	REQUIRE(std::get<ObjectName>(r.specification) == ObjectName::Domain("DOMAIN_A", "DS_TEST_001"));
	RoundTrip(ReadReqListName);
	RequireEncodes(ConfirmedRequest{0x1234, r}, ReadReqListName);

	const ReadResponse resp = Service<ReadResponse>(Decoded<ConfirmedResponse>(ReadResp));
	REQUIRE_FALSE(resp.specification.has_value());
	REQUIRE(resp.results.size() == 6);
	REQUIRE(resp.results[0].data.AsString() == "VALUE_1");
	REQUIRE(resp.results[1].data.type == DataType::BitString);
	REQUIRE(resp.results[1].data.AsBits() == MakeBits({0, 2}, 12));
	REQUIRE(resp.results[2].data.type == DataType::Structure);
	REQUIRE(resp.results[2].data.elements[0].AsInteger() == 2001);
	REQUIRE(resp.results[2].data.elements[1].AsInteger() == 7);
	REQUIRE_FALSE(resp.results[3].success);
	REQUIRE(resp.results[3].error == DataAccessError::ObjectNonExistent);
	REQUIRE(resp.results[4].data.AsBoolean());
	REQUIRE(resp.results[5].data.AsUnsigned() == 200);
	RoundTrip(ReadResp);
	ReadResponse built;
	built.results = {AccessResult::Success(Data::VisibleString("VALUE_1")),
		           AccessResult::Success(Data::Bits(MakeBits({0, 2}, 12))),
		           AccessResult::Success(Data::Structure({Data::Integer(2001), Data::Integer(7)})),
		           AccessResult::Failure(DataAccessError::ObjectNonExistent), AccessResult::Success(Data::Boolean(true)),
		           AccessResult::Success(Data::Unsigned(200))};
	RequireEncodes(ConfirmedResponse{0x1234, built}, ReadResp);

	//a response that echoes the specification
	ReadResponse echo;
	echo.specification = ThreeVariables();
	echo.results = {AccessResult::Success(Data::Integer(-5))};
	const Bytes wire = Encode(ConfirmedResponse{5, echo});
	REQUIRE(std::get<ConfirmedResponse>(Decode(wire)) == ConfirmedResponse{5, echo});
}

TEST_CASE(SUITE("Write: the 12 member structure and the responses"))
{
	const WriteRequest w = Service<WriteRequest>(Decoded<ConfirmedRequest>(WriteReq12));
	REQUIRE(std::get<ListOfVariable>(w.specification).size() == 1);
	REQUIRE(std::get<ListOfVariable>(w.specification)[0].name == ObjectName::Domain("DOMAIN_A", "TS_TEST_01"));
	REQUIRE(w.data.size() == 1);
	const Data& t = w.data[0];
	REQUIRE(t.type == DataType::Structure);
	REQUIRE(t.elements.size() == 12);
	REQUIRE(t.elements[0].type == DataType::Structure);
	REQUIRE(t.elements[0].elements.size() == 3);
	REQUIRE(t.elements[0].elements[0].AsUnsigned() == 1);
	REQUIRE(t.elements[0].elements[1].AsString() == "DOMAIN_A");
	REQUIRE(t.elements[0].elements[2].AsString() == "DS_TEST_001");
	REQUIRE(t.elements[1].AsInteger() == 0);
	REQUIRE(t.elements[2].AsInteger() == 5);
	REQUIRE(t.elements[5].AsInteger() == 600);
	REQUIRE(t.elements[6].type == DataType::BitString);
	REQUIRE(t.elements[6].AsBits() == MakeBits({0, 1}, 5));
	REQUIRE_FALSE(t.elements[7].AsBoolean());
	REQUIRE(t.elements[9].AsBoolean());
	REQUIRE(t.elements[10].AsBoolean());
	REQUIRE(t.elements[11].AsInteger() == 0);
	RoundTrip(WriteReq12);

	WriteRequest built;
	built.specification = ListOfVariable{VariableSpec{ObjectName::Domain("DOMAIN_A", "TS_TEST_01"), std::nullopt}
	};
	built.data = {Data::Structure({Data::Structure({Data::Unsigned(1), Data::VisibleString("DOMAIN_A"), Data::VisibleString("DS_TEST_001")}),
		                         Data::Integer(0), Data::Integer(5), Data::Integer(0), Data::Integer(0), Data::Integer(600),
		                         Data::Bits(MakeBits({0, 1}, 5)), Data::Boolean(false), Data::Boolean(false), Data::Boolean(true), Data::Boolean(true),
		                         Data::Integer(0)})};
	RequireEncodes(ConfirmedRequest{0x1234, built}, WriteReq12);
	REQUIRE(built == w);

	const WriteResponse ok = Service<WriteResponse>(Decoded<ConfirmedResponse>(WriteRespOk));
	REQUIRE(ok.results == std::vector<WriteResult>{WriteResult{true, 0}
		});
	RoundTrip(WriteRespOk);
	RequireEncodes(ConfirmedResponse{0x1234, ok}, WriteRespOk);
	const WriteResponse mixed = Service<WriteResponse>(Decoded<ConfirmedResponse>(WriteRespMixed));
	REQUIRE(mixed.results == std::vector<WriteResult>{WriteResult{true, 0}, WriteResult{false, DataAccessError::ObjectAccessDenied}
		});
	RoundTrip(WriteRespMixed);

	//a write addressed to a variable list name
	WriteRequest byList;
	byList.specification = ObjectName::Vmd("LIST_X");
	byList.data = {Data::Integer(1), Data::Integer(2)};
	const Bytes wire = Encode(ConfirmedRequest{3, byList});
	REQUIRE(std::get<ConfirmedRequest>(Decode(wire)) == ConfirmedRequest{3, byList});
}

TEST_CASE(SUITE("DefineNamedVariableList, DeleteNamedVariableList and GetNamedVariableListAttributes"))
{
	const ListOfVariable members{VariableSpec{ObjectName::Domain("DOMAIN_A", "POINT_0001_MW"), std::nullopt},
		                       VariableSpec{ObjectName::Vmd("POINT_0002_KV"), std::nullopt}
	};

	const DefineNamedVariableListRequest def = Service<DefineNamedVariableListRequest>(Decoded<ConfirmedRequest>(DefNvlReq));
	REQUIRE(def.name == ObjectName::Domain("DOMAIN_A", "DS_TEST_001"));
	REQUIRE(def.variables == members);
	RoundTrip(DefNvlReq);
	RequireEncodes(ConfirmedRequest{0x1234, def}, DefNvlReq);
	REQUIRE(std::holds_alternative<DefineNamedVariableListResponse>(Decoded<ConfirmedResponse>(DefNvlResp).service));
	RoundTrip(DefNvlResp);
	RequireEncodes(ConfirmedResponse{0x1234, DefineNamedVariableListResponse{}
		}, DefNvlResp);

	DeleteNamedVariableListRequest del = Service<DeleteNamedVariableListRequest>(Decoded<ConfirmedRequest>(DelNvlReq));
	REQUIRE_FALSE(del.scopeOfDelete.has_value());
	REQUIRE(del.listOfVariableListName->size() == 1);
	REQUIRE((*del.listOfVariableListName)[0] == ObjectName::Domain("DOMAIN_A", "DS_TEST_001"));
	RoundTrip(DelNvlReq);
	RequireEncodes(ConfirmedRequest{0x1234, del}, DelNvlReq);
	del = Service<DeleteNamedVariableListRequest>(Decoded<ConfirmedRequest>(DelNvlReqScope));
	REQUIRE(del.scopeOfDelete == DeleteScope::Domain);
	REQUIRE_FALSE(del.listOfVariableListName.has_value());
	REQUIRE(del.domainName == "DOMAIN_A");
	RoundTrip(DelNvlReqScope);
	RequireEncodes(ConfirmedRequest{0x1234, del}, DelNvlReqScope);
	const DeleteNamedVariableListResponse delResp = Service<DeleteNamedVariableListResponse>(Decoded<ConfirmedResponse>(DelNvlResp));
	REQUIRE(delResp.numberMatched == 1);
	REQUIRE(delResp.numberDeleted == 1);
	RoundTrip(DelNvlResp);
	RequireEncodes(ConfirmedResponse{0x1234, delResp}, DelNvlResp);

	const GetNamedVariableListAttributesRequest ga = Service<GetNamedVariableListAttributesRequest>(Decoded<ConfirmedRequest>(GetNvlAttrReq));
	REQUIRE(ga.name == ObjectName::Domain("DOMAIN_A", "DS_TEST_001"));
	RoundTrip(GetNvlAttrReq);
	RequireEncodes(ConfirmedRequest{0x1234, ga}, GetNvlAttrReq);
	const GetNamedVariableListAttributesResponse gr = Service<GetNamedVariableListAttributesResponse>(Decoded<ConfirmedResponse>(GetNvlAttrResp));
	REQUIRE(gr.mmsDeletable);
	REQUIRE(gr.variables == members);
	REQUIRE_FALSE(gr.accessControlListName.has_value());
	RoundTrip(GetNvlAttrResp);
	RequireEncodes(ConfirmedResponse{0x1234, gr}, GetNvlAttrResp);
}

TEST_CASE(SUITE("DefineNamedVariableList with hundreds of members"))
{
	for(size_t count : {size_t(2), size_t(501), size_t(3000)})
	{
		DefineNamedVariableListRequest def;
		def.name = ObjectName::Domain("DOMAIN_A", "DS_TEST_001");
		size_t expected = 0;
		for(size_t i = 0; i < count; ++i)
		{
			if(i % 3 == 0)
			{
				def.variables.push_back(VariableSpec{ObjectName::Domain("DOMAIN_B", PointName(i)), std::nullopt});
				expected += 2 + 2 + 2 + (2 + 8) + (2 + 13); //30 a0 a1 1a 1a
			}
			else
			{
				def.variables.push_back(VariableSpec{ObjectName::Vmd(PointName(i, "KV")), std::nullopt});
				expected += 2 + 2 + 2 + 13; //30 a0 80
			}
		}
		const Bytes wire = Encode(ConfirmedRequest{0x1234, def});
		const ConfirmedRequest back = std::get<ConfirmedRequest>(Decode(wire));
		REQUIRE(Service<DefineNamedVariableListRequest>(back).variables.size() == count);
		REQUIRE(back == ConfirmedRequest{0x1234, def});
		REQUIRE(Encode(back) == wire);

		//the list [0] header length matches the independent expectation
		const size_t nameLen = 2 + (2 + 8) + (2 + 11);
		const Bytes listHeader = Cat({Bytes{0xa0}, Len(expected)});
		const Bytes svcHeader = Cat({Bytes{0xab}, Len(nameLen + listHeader.size() + expected)});
		const Bytes pduHeader = Cat({Bytes{0xa0}, Len(4 + svcHeader.size() + nameLen + listHeader.size() + expected)});
		REQUIRE(wire.size() == pduHeader.size() + 4 + svcHeader.size() + nameLen + listHeader.size() + expected);
		REQUIRE(Bytes(wire.begin(), wire.begin() + pduHeader.size()) == pduHeader);
	}
}

TEST_CASE(SUITE("GetVariableAccessAttributes and type descriptions"))
{
	const GetVariableAccessAttributesRequest rq = Service<GetVariableAccessAttributesRequest>(Decoded<ConfirmedRequest>(GvaaReq));
	REQUIRE(rq.name == ObjectName::Domain("DOMAIN_A", "POINT_0001_MW"));
	RoundTrip(GvaaReq);
	RequireEncodes(ConfirmedRequest{0x1234, rq}, GvaaReq);

	const GetVariableAccessAttributesResponse rs = Service<GetVariableAccessAttributesResponse>(Decoded<ConfirmedResponse>(GvaaResp));
	REQUIRE_FALSE(rs.mmsDeletable);
	const TypeDescription& t = rs.typeDescription;
	REQUIRE(t.kind == TypeDescription::Kind::Structure);
	REQUIRE(t.subtypes.size() == 2);
	REQUIRE(t.componentNames[0] == "ID");
	REQUIRE(t.subtypes[0].kind == TypeDescription::Kind::Integer);
	REQUIRE(t.subtypes[0].size == 32);
	REQUIRE(t.componentNames[1] == "V");
	REQUIRE(t.subtypes[1].kind == TypeDescription::Kind::FloatingPoint);
	REQUIRE(t.subtypes[1].formatWidth == 32);
	REQUIRE(t.subtypes[1].exponentWidth == 8);
	RoundTrip(GvaaResp);
	RequireEncodes(ConfirmedResponse{0x1234, rs}, GvaaResp);
}

TEST_CASE(SUITE("Every TypeDescription alternative round trips"))
{
	using Kind = TypeDescription::Kind;
	auto scalar = [](Kind k, int64_t size = 0)
			  {
				  TypeDescription t;
				  t.kind = k;
				  t.size = size;
				  return t;
			  };
	TypeDescription fp;
	fp.kind = Kind::FloatingPoint;
	fp.formatWidth = 64;
	fp.exponentWidth = 11;
	TypeDescription bt;
	bt.kind = Kind::BinaryTime;
	bt.timeOfDay = true;
	TypeDescription named;
	named.kind = Kind::TypeName;
	named.typeName = ObjectName::Domain("DOMAIN_A", "TYPE_X");
	TypeDescription arr;
	arr.kind = Kind::Array;
	arr.packed = true;
	arr.size = 12;
	arr.subtypes = {scalar(Kind::Unsigned, 16)};
	TypeDescription st;
	st.kind = Kind::Structure;
	st.subtypes = {scalar(Kind::Boolean), arr, fp, named};
	st.componentNames = {std::nullopt, std::string("ARR"), std::string("F"), std::nullopt};

	for(const TypeDescription& t : {scalar(Kind::Boolean), scalar(Kind::BitString, 13), scalar(Kind::Integer, 8), scalar(Kind::Unsigned, 32),
	                                fp, scalar(Kind::OctetString, 16), scalar(Kind::VisibleString, 32), scalar(Kind::GeneralizedTime), bt,
	                                scalar(Kind::Bcd, 4), scalar(Kind::ObjectId), scalar(Kind::MmsString, 20), scalar(Kind::UtcTime), named, arr, st})
	{
		GetVariableAccessAttributesResponse r;
		r.mmsDeletable = true;
		r.typeDescription = t;
		r.accessControlListName = "ACL_X";
		const Bytes wire = Encode(ConfirmedResponse{77, r});
		const ConfirmedResponse back = std::get<ConfirmedResponse>(Decode(wire));
		REQUIRE(Service<GetVariableAccessAttributesResponse>(back) == r);
		REQUIRE(Encode(back) == wire);
	}
}

TEST_CASE(SUITE("Unconfirmed InformationReport"))
{
	const InformationReport r = Decoded<InformationReport>(InfoReport);
	REQUIRE(std::get<ObjectName>(r.specification) == ObjectName::Domain("DOMAIN_A", "DS_TEST_001"));
	REQUIRE(r.results.size() == 4);
	REQUIRE(r.results[0].data.type == DataType::Structure);
	REQUIRE(r.results[0].data.elements[0].AsUnsigned() == 1);
	REQUIRE(r.results[0].data.elements[1].AsString() == "DOMAIN_A");
	REQUIRE(r.results[0].data.elements[2].AsString() == "TS_TEST_01");
	REQUIRE(r.results[1].data.elements[0].AsFloat() == Approx(123.456).epsilon(1e-6));
	REQUIRE(r.results[1].data.elements[1].AsBits() == MakeBits({}, 8));
	REQUIRE(r.results[2].data.AsInteger() == 0);
	REQUIRE(r.results[3].data.AsBits() == MakeBits({0}, 8));
	RoundTrip(InfoReport);

	InformationReport built;
	built.specification = ObjectName::Domain("DOMAIN_A", "DS_TEST_001");
	built.results = {AccessResult::Success(Data::Structure({Data::Unsigned(1), Data::VisibleString("DOMAIN_A"), Data::VisibleString("TS_TEST_01")})),
		           AccessResult::Success(Data::Structure({Data::Float32(123.456f), Data::Bits(MakeBits({}, 8))})),
		           AccessResult::Success(Data::Integer(0)), AccessResult::Success(Data::Bits(MakeBits({0}, 8)))};
	RequireEncodes(built, InfoReport);

	//listOfVariable form
	InformationReport byVars;
	byVars.specification = ThreeVariables();
	byVars.results = {AccessResult::Success(Data::Integer(1)), AccessResult::Failure(DataAccessError::HardwareFault), AccessResult::Success(Data::Boolean(false))};
	const Bytes wire = Encode(byVars);
	REQUIRE(std::get<InformationReport>(Decode(wire)) == byVars);
}

TEST_CASE(SUITE("InformationReport with hundreds of real and state values"))
{
	InformationReport rep;
	rep.specification = ObjectName::Domain("DOMAIN_A", "DS_TEST_002");
	rep.results.push_back(AccessResult::Success(Data::Structure({Data::Unsigned(1), Data::VisibleString("DOMAIN_A"), Data::VisibleString("TS_TEST_01")})));
	size_t expected = 2 + 3 + 10 + 12; //first element: a2 19 | 86 01 01 | 8a 08 .. | 8a 0a ..
	for(size_t i = 0; i < 885; ++i)
	{
		rep.results.push_back(AccessResult::Success(Data::Structure({Data::Float32(static_cast<float>(i) * 1.5f), Data::Bits(MakeBits({}, 8))})));
		expected += 2 + 7 + 4; //a2 0b | 87 05 08 xxxxxxxx | 84 02 00 00
	}
	for(size_t i = 0; i < 777; ++i)
	{
		rep.results.push_back(AccessResult::Success(Data::Bits(MakeBits({i % 8}, 8))));
		expected += 4;
	}
	const Bytes wire = Encode(rep);
	const InformationReport back = std::get<InformationReport>(Decode(wire));
	REQUIRE(back.results.size() == 1 + 885 + 777);
	REQUIRE(back == rep);
	REQUIRE(Encode(back) == wire);
	REQUIRE(back.results[2].data.elements[0].AsFloat() == 1.5);

	//independent length of the [0] listOfAccessResult header
	const Bytes listHeader = Cat({Bytes{0xa0}, Len(expected)});
	const size_t specLen = 2 + 2 + (2 + 8) + (2 + 11); //a1 | a1 | 1a 1a
	const size_t reportContent = specLen + listHeader.size() + expected;
	const Bytes reportHeader = Cat({Bytes{0xa0}, Len(reportContent)});
	const Bytes pduHeader = Cat({Bytes{0xa3}, Len(reportHeader.size() + reportContent)});
	REQUIRE(wire.size() == pduHeader.size() + reportHeader.size() + reportContent);
	REQUIRE(Bytes(wire.begin(), wire.begin() + pduHeader.size()) == pduHeader);
}

TEST_CASE(SUITE("Data factories and accessors"))
{
	//integers
	REQUIRE(EncodeData(Data::Integer(0)) == Hex("85 01 00"));
	REQUIRE(EncodeData(Data::Integer(127)) == Hex("85 01 7f"));
	REQUIRE(EncodeData(Data::Integer(128)) == Hex("85 02 00 80"));
	REQUIRE(EncodeData(Data::Integer(-1)) == Hex("85 01 ff"));
	REQUIRE(EncodeData(Data::Integer(-128)) == Hex("85 01 80"));
	REQUIRE(EncodeData(Data::Integer(-129)) == Hex("85 02 ff 7f"));
	REQUIRE(EncodeData(Data::Integer(INT64_MIN)) == Hex("85 08 80 00 00 00 00 00 00 00"));
	REQUIRE(EncodeData(Data::Integer(INT64_MAX)) == Hex("85 08 7f ff ff ff ff ff ff ff"));
	for(int64_t v : {int64_t(0), int64_t(1), int64_t(-1), int64_t(255), int64_t(-256), int64_t(65535), INT64_MIN, INT64_MAX})
		REQUIRE(DecodeData(EncodeData(Data::Integer(v))).AsInteger() == v);

	//unsigned
	REQUIRE(EncodeData(Data::Unsigned(0)) == Hex("86 01 00"));
	REQUIRE(EncodeData(Data::Unsigned(200)) == Hex("86 02 00 c8"));
	REQUIRE(EncodeData(Data::Unsigned(UINT64_MAX)) == Hex("86 09 00 ff ff ff ff ff ff ff ff"));
	for(uint64_t v : {uint64_t(0), uint64_t(127), uint64_t(128), uint64_t(0xFFFFFFFF), UINT64_MAX})
		REQUIRE(DecodeData(EncodeData(Data::Unsigned(v))).AsUnsigned() == v);

	//booleans are written as 01 / 00
	REQUIRE(EncodeData(Data::Boolean(true)) == Hex("83 01 01"));
	REQUIRE(EncodeData(Data::Boolean(false)) == Hex("83 01 00"));

	//floating point
	REQUIRE(EncodeData(Data::Float32(123.456f)) == Hex("87 05 08 42 f6 e9 79"));
	REQUIRE(EncodeData(Data::Float32(-0.0f)) == Hex("87 05 08 80 00 00 00"));
	REQUIRE(EncodeData(Data::Float64(1.5)) == Hex("87 09 0b 3f f8 00 00 00 00 00 00"));
	REQUIRE(DecodeData(Hex("87 05 08 42 f6 e9 79")).AsFloat() == Approx(123.456).epsilon(1e-6));
	REQUIRE(DecodeData(Hex("87 09 0b 3f f8 00 00 00 00 00 00")).AsFloat() == 1.5);
	REQUIRE(DecodeData(Hex("87 05 08 7f 80 00 00")).AsFloat() == std::numeric_limits<double>::infinity());
	//other FloatingPoint formats are carried but cannot be converted
	const Data odd = DecodeData(Hex("87 04 07 01 02 03"));
	REQUIRE_THROWS_AS(odd.AsFloat(), iccp::ber::DecodeError);
	REQUIRE(EncodeData(odd) == Hex("87 04 07 01 02 03"));

	//strings and bit strings
	REQUIRE(EncodeData(Data::VisibleString("AB")) == Hex("8a 02 41 42"));
	REQUIRE(EncodeData(Data::MmsString("AB")) == Hex("90 02 41 42"));
	REQUIRE(EncodeData(Data::OctetString(Hex("01 02"))) == Hex("89 02 01 02"));
	REQUIRE(EncodeData(Data::GeneralizedTime("20240101000000Z")) == Hex("8b 0f 32 30 32 34 30 31 30 31 30 30 30 30 30 30 5a"));
	REQUIRE(EncodeData(Data::Bits(MakeBits({0, 1}, 5))) == Hex("84 02 03 c0"));
	REQUIRE(EncodeData(Data::BitsArray(MakeBits({7}, 8))) == Hex("8e 02 00 01"));
	REQUIRE(DecodeData(Hex("84 02 03 c0")).AsBits() == MakeBits({0, 1}, 5));
	REQUIRE(DecodeData(Hex("8e 02 00 01")).AsBits().Get(7));
	REQUIRE(DecodeData(Hex("89 02 01 02")).AsOctetString() == Hex("01 02"));
	REQUIRE(DecodeData(Hex("90 02 41 42")).AsString() == "AB");

	//BCD, object id
	REQUIRE(EncodeData(Data::Bcd(42)) == Hex("8d 01 2a"));
	REQUIRE(DecodeData(Hex("8d 01 2a")).AsBcd() == 42);
	REQUIRE(EncodeData(Data::ObjectId(Oid{1, 0, 9506, 2, 1})) == Hex("8f 05 28 ca 22 02 01"));
	REQUIRE(DecodeData(Hex("8f 05 28 ca 22 02 01")).AsObjectId() == Oid({1, 0, 9506, 2, 1}));

	//time values
	const UtcTime utc{0x5F5E1000u, 0x800000u, 0x0A};
	REQUIRE(EncodeData(Data::Utc(utc)) == Hex("91 08 5f 5e 10 00 80 00 00 0a"));
	REQUIRE(DecodeData(Hex("91 08 5f 5e 10 00 80 00 00 0a")).AsUtc() == utc);
	REQUIRE_THROWS_AS(Data::Utc(UtcTime{0, 0x1000000u, 0}), std::invalid_argument);
	BinaryTime bt{43200000u, std::nullopt};
	REQUIRE(EncodeData(Data::Binary(bt)) == Hex("8c 04 02 93 2e 00"));
	REQUIRE(DecodeData(Hex("8c 04 02 93 2e 00")).AsBinary() == bt);
	bt.days = 14000;
	REQUIRE(EncodeData(Data::Binary(bt)) == Hex("8c 06 02 93 2e 00 36 b0"));
	REQUIRE(DecodeData(Hex("8c 06 02 93 2e 00 36 b0")).AsBinary() == bt);

	//array and structure, with nesting
	const Data nested = Data::Array({Data::Structure({Data::Integer(1), Data::Array({})}), Data::Structure({})});
	REQUIRE(EncodeData(nested) == Hex("a1 09 a2 05 85 01 01 a1 00 a2 00"));
	REQUIRE(DecodeData(EncodeData(nested)) == nested);

	//the accessors reject the wrong type
	REQUIRE_THROWS_AS(Data::Integer(1).AsBoolean(), iccp::ber::DecodeError);
	REQUIRE_THROWS_AS(Data::Boolean(true).AsInteger(), iccp::ber::DecodeError);
	REQUIRE_THROWS_AS(Data::Integer(1).AsString(), iccp::ber::DecodeError);
	REQUIRE_THROWS_AS(Data::Integer(1).AsBits(), iccp::ber::DecodeError);
	REQUIRE_THROWS_AS(Data::Integer(1).AsUtc(), iccp::ber::DecodeError);
}

TEST_CASE(SUITE("Encodings seen from peers are decoded leniently and preserved"))
{
	//BOOLEAN TRUE as 0x01 and as 0xff, a non-minimal integer, a non-minimal unsigned: all preserved
	const char* odd = "a1 14 83 01 ff 83 01 01 85 04 00 00 00 05 86 03 00 00 07 83 01 02";
	const Data d = DecodeData(Hex(odd));
	REQUIRE(d.elements.size() == 5);
	REQUIRE(d.elements[0].AsBoolean());
	REQUIRE(d.elements[1].AsBoolean());
	REQUIRE(d.elements[2].AsInteger() == 5);
	REQUIRE(d.elements[3].AsUnsigned() == 7);
	REQUIRE(d.elements[4].AsBoolean());
	REQUIRE(EncodeData(d) == Hex(odd));

	//moreFollows 0xff and mmsDeletable 0xff are accepted
	const Bytes wire = Hex("a1 0a 02 01 01 a1 05 a0 00 81 01 ff");
	REQUIRE(std::get<ConfirmedResponse>(Decode(wire)) == ConfirmedResponse{1, GetNameListResponse{
													     {}, true}
		});
	//but protocol booleans are written as 01
	REQUIRE(Encode(ConfirmedResponse{1, GetNameListResponse{
							   {}, true}
		}) == Hex("a1 0a 02 01 01 a1 05 a0 00 81 01 01"));

	//indefinite lengths are accepted (and re-encoded definite)
	const Bytes indef = Hex("a1 80 02 01 01 a1 80 a0 80 1a 01 41 00 00 00 00 00 00");
	const ConfirmedResponse r = std::get<ConfirmedResponse>(Decode(indef));
	REQUIRE(Service<GetNameListResponse>(r).identifiers == std::vector<std::string>{"A"});
	REQUIRE(Encode(r) == Hex("a1 0a 02 01 01 a1 05 a0 03 1a 01 41"));

	//unknown trailing optional elements inside a SEQUENCE are skipped: cs-request-detail [79] after the service
	const Bytes extra = Hex("a0 0b 02 01 05 82 00 bf 4f 03 80 01 00");
	REQUIRE(std::get<ConfirmedRequest>(Decode(extra)) == ConfirmedRequest{5, IdentifyRequest{}
		});
}

TEST_CASE(SUITE("Unsupported requests can still be answered via PeekInvokeId"))
{
	//a Status request (service [0]) is unsupported
	const Bytes status = Hex("a0 06 02 02 12 34 80 00");
	REQUIRE_THROWS_AS(Decode(status), iccp::ber::DecodeError);
	REQUIRE(PeekInvokeId(status) == 0x1234u);

	//listOfModifier
	const Bytes modifier = Hex("a0 07 02 01 07 30 00 82 00");
	REQUIRE_THROWS_AS(Decode(modifier), iccp::ber::DecodeError);
	REQUIRE(PeekInvokeId(modifier) == 7u);

	//a malformed request body still yields the id
	const Bytes broken = Hex("a0 07 02 02 12 34 a1 01 ff");
	REQUIRE_THROWS_AS(Decode(broken), iccp::ber::DecodeError);
	REQUIRE(PeekInvokeId(broken) == 0x1234u);

	REQUIRE(PeekInvokeId(Hex(IdentResp)) == 0x1234u);
	REQUIRE(PeekInvokeId(Hex(ConfErr)) == 0x1234u);
	REQUIRE(PeekInvokeId(Hex(RejectPdu)) == 0x1234u);
	REQUIRE_FALSE(PeekInvokeId(Hex(RejectNoId)).has_value());
	REQUIRE_FALSE(PeekInvokeId(Hex(InfoReport)).has_value());
	REQUIRE_FALSE(PeekInvokeId(Hex(ConcludeReqPdu)).has_value());
	REQUIRE_FALSE(PeekInvokeId(Bytes()).has_value());
	REQUIRE_FALSE(PeekInvokeId(Hex("a0 03 02")).has_value());
	REQUIRE_FALSE(PeekInvokeId(Hex("a0 07 02 05 01 00 00 00 00")).has_value()); //does not fit in 32 bits

	//unsupported PDU types and services
	REQUIRE_THROWS_AS(Decode(Hex("a5 03 80 01 01")), iccp::ber::DecodeError);          //cancel-RequestPDU
	REQUIRE_THROWS_AS(Decode(Hex("a3 04 a1 02 80 00")), iccp::ber::DecodeError);       //unsolicited status
	REQUIRE_THROWS_AS(Decode(Hex("a1 06 02 01 01 a3 01 00")), iccp::ber::DecodeError); //response to rename
	REQUIRE_THROWS_AS(Decode(Hex("60 00")), iccp::ber::DecodeError);                   //not an MMS PDU
	REQUIRE_THROWS_AS(Decode(Hex("")), iccp::ber::DecodeError);
	REQUIRE_THROWS_AS(Decode(nullptr, 3), std::invalid_argument);
}

TEST_CASE(SUITE("Decode rejects malformed PDUs"))
{
	for(const char* bad : {
		"a0 06 02 02 12 34 82 00 00",                         //trailing octet
		"a0 05 02 02 12 34 82",                               //truncated element
		"a0 0b 02 01 05 82 01 00",                            //identify with content
		"a0 06 04 02 12 34 82 00",                            //invoke id is not an INTEGER
		"a0 09 02 05 01 00 00 00 00 82 00",                   //invoke id over 32 bits
		"a0 05 02 01 ff 82 00",                               //negative invoke id
		"a0 05 02 00 82 00 00",                               //empty INTEGER
		"a0 02 82 00",                                        //no invoke id
		"a0 08 02 01 01 a1 03 a0 01 02",                      //GetNameList without scope
		"a0 0c 02 01 01 a1 07 a0 03 81 01 00 a1 00",          //extended object class
		"a0 0e 02 01 01 a1 09 a0 03 80 01 00 a1 02 83 00",    //unknown scope alternative
		"a0 0f 02 01 01 a4 0a a1 08 a0 06 30 04 a1 02 80 00", //variable specification other than name
		"a0 0c 02 01 01 a4 07 a1 05 a0 03 04 01 00",          //listOfVariable entry is not a SEQUENCE
		"a0 05 02 01 01 a4 00",                               //Read without specification
		"a0 0d 02 01 01 a5 08 a1 06 a0 04 83 02 00 00",       //boolean with two octets
		"a0 0a 02 01 01 a5 05 a1 03 80 01 01",                //write specification with the wrong list tag
		"a1 0c 02 01 01 a1 07 a0 05 04 03 41 42 43",          //identifier is not a VisibleString
		"a1 0b 02 01 01 a1 06 a0 04 1a 02 41",                //name element longer than its list
		"a1 08 02 01 01 a5 03 82 01 00",                      //unknown write result alternative
		"a2 04 80 01 01 80",                                  //error without serviceError
		"a2 0b 80 01 01 a2 06 a0 04 8d 01 00 00",             //unknown errorClass 13
		"a4 03 8c 01 00",                                     //reject type 12
		"a8 05 81 01 0a 82 01",                               //initiate without detail
		"8b 01 00",                                           //conclude with content
		"ad 00",                                              //conclude error without errorClass
		"aa 05 a0 03 88 01 ff",                               //negative error code
	})
	{
		INFO(bad);
		REQUIRE_THROWS_AS(Decode(Hex(bad)), iccp::ber::DecodeError);
	}
}

TEST_CASE(SUITE("Decode limits: nesting, data nodes and list sizes"))
{
	//nesting: structures nested n deep around an integer
	auto nest = [](size_t depth)
			{
				Bytes b = Hex("85 01 01");
				for(size_t i = 0; i < depth; ++i)
					b = Tlv(0xa2, b);
				return b;
			};
	REQUIRE_NOTHROW(DecodeData(nest(MaxNesting)));
	REQUIRE_THROWS_AS(DecodeData(nest(MaxNesting + 1)), iccp::ber::DecodeError);
	REQUIRE_THROWS_AS(DecodeData(nest(5000)), iccp::ber::DecodeError);
	//and the encoder holds to the same cap
	Data deep = Data::Integer(1);
	for(size_t i = 0; i < MaxNesting; ++i)
		deep = Data::Structure({deep});
	REQUIRE_NOTHROW(EncodeData(deep));
	deep = Data::Structure({deep});
	REQUIRE_THROWS_AS(EncodeData(deep), std::invalid_argument);

	//a flat list at the cap is accepted and one over it is not
	Bytes flat;
	for(size_t i = 0; i < MaxListEntries; ++i)
		flat.insert(flat.end(), {0x83, 0x01, 0x00});
	REQUIRE_NOTHROW(DecodeData(Tlv(0xa2, flat)));
	flat.insert(flat.end(), {0x83, 0x01, 0x00});
	REQUIRE_THROWS_AS(DecodeData(Tlv(0xa2, flat)), iccp::ber::DecodeError);

	//the total node count is capped even when no single list is too long
	Bytes third;
	for(size_t i = 0; i < MaxListEntries; ++i)
		third.insert(third.end(), {0x83, 0x01, 0x00});
	const Bytes inner = Tlv(0xa2, third);
	REQUIRE_THROWS_AS(DecodeData(Tlv(0xa2, Cat({inner, inner, inner}))), iccp::ber::DecodeError);

	//too many names in a page
	Bytes names;
	for(size_t i = 0; i < MaxListEntries + 1; ++i)
		names.insert(names.end(), {0x1a, 0x01, 0x41});
	const Bytes page = Tlv(0xa1, Cat({Hex("02 01 01"), Tlv(0xa1, Tlv(0xa0, names))}));
	REQUIRE_THROWS_AS(Decode(page), iccp::ber::DecodeError);

	//identifier length cap
	REQUIRE_NOTHROW(Decode(Tlv(0xa0, Hex("02 01 01 a1 09 a0 03 80 01 00 a1 02 81 00"))));
	GetNameListRequest r;
	r.continueAfter = std::string(MaxIdentifierLength, 'x');
	REQUIRE_NOTHROW(Decode(Encode(ConfirmedRequest{1, r})));
	r.continueAfter = std::string(MaxIdentifierLength + 1, 'x');
	REQUIRE_THROWS_AS(Encode(ConfirmedRequest{1, r}), std::invalid_argument);
	const Bytes longId = Tlv(0xa0, Cat({Hex("02 01 01"), Tlv(0xa1, Cat({Hex("a0 03 80 01 00 a1 02 80 00"), Tlv(0x82, Bytes(MaxIdentifierLength + 1, 'x'))}))}));
	REQUIRE_THROWS_AS(Decode(longId), iccp::ber::DecodeError);
}

TEST_CASE(SUITE("Encode rejects what cannot be encoded"))
{
	GetNameListRequest g;
	g.objectClass = -1;
	REQUIRE_THROWS_AS(Encode(ConfirmedRequest{1, g}), std::invalid_argument);

	Reject rj;
	rj.type = 0;
	REQUIRE_THROWS_AS(Encode(rj), std::invalid_argument);
	rj.type = 12;
	REQUIRE_THROWS_AS(Encode(rj), std::invalid_argument);
	rj.type = 1;
	rj.reason = -1;
	REQUIRE_THROWS_AS(Encode(rj), std::invalid_argument);

	ConfirmedError ce;
	ce.error.errorClass = 13;
	REQUIRE_THROWS_AS(Encode(ce), std::invalid_argument);
	ce.error.errorClass = ErrorClass::Others;
	ce.error.errorCode = -1;
	REQUIRE_THROWS_AS(Encode(ce), std::invalid_argument);
	ce.error.errorCode = 0;
	ce.error.serviceSpecificInformation = Hex("80 00");
	REQUIRE_THROWS_AS(Encode(ce), std::invalid_argument);

	InitiateRequest ir;
	ir.proposedMaxServOutstandingCalling = -1;
	REQUIRE_THROWS_AS(Encode(ir), std::invalid_argument);

	//scalar Data with content that does not fit its type
	Data bad = Data::Boolean(true);
	bad.content = Hex("01 02");
	REQUIRE_THROWS_AS(EncodeData(bad), std::invalid_argument);
	bad = Data::Utc(UtcTime{});
	bad.content.pop_back();
	REQUIRE_THROWS_AS(EncodeData(bad), std::invalid_argument);
	bad = Data::Structure({});
	bad.content = Hex("00");
	REQUIRE_THROWS_AS(EncodeData(bad), std::invalid_argument);
	bad = Data::Integer(1);
	bad.type = static_cast<DataType>(8);
	REQUIRE_THROWS_AS(EncodeData(bad), std::invalid_argument);

	VariableSpec vs{ObjectName::Vmd("X"), Hex("a5 01")};
	REQUIRE_THROWS_AS(Encode(ConfirmedRequest{1, DefineNamedVariableListRequest{ObjectName::Vmd("L"), {vs}
								}
		}), std::invalid_argument);
	vs.alternateAccess = Hex("86 00");
	REQUIRE_THROWS_AS(Encode(ConfirmedRequest{1, DefineNamedVariableListRequest{ObjectName::Vmd("L"), {vs}
								}
		}), std::invalid_argument);
	vs.alternateAccess = Hex("a5 00");
	REQUIRE_NOTHROW(Encode(ConfirmedRequest{1, DefineNamedVariableListRequest{ObjectName::Vmd("L"), {vs}
							    }
		}));

	DeleteNamedVariableListRequest del;
	del.scopeOfDelete = 4;
	REQUIRE_THROWS_AS(Encode(ConfirmedRequest{1, del}), std::invalid_argument);

	TypeDescription t;
	t.kind = TypeDescription::Kind::TypeName;
	GetVariableAccessAttributesResponse gr;
	gr.typeDescription = t;
	REQUIRE_THROWS_AS(Encode(ConfirmedResponse{1, gr}), std::invalid_argument);
	t.kind = TypeDescription::Kind::Array;
	gr.typeDescription = t;
	REQUIRE_THROWS_AS(Encode(ConfirmedResponse{1, gr}), std::invalid_argument);
	t.kind = TypeDescription::Kind::Structure;
	t.subtypes.resize(1);
	gr.typeDescription = t;
	REQUIRE_THROWS_AS(Encode(ConfirmedResponse{1, gr}), std::invalid_argument);
}

TEST_CASE(SUITE("Alternate access is carried opaquely"))
{
	ReadRequest r;
	VariableSpec v{ObjectName::Vmd("ARRAY_X"), Hex("a5 05 a1 03 80 01 02")};
	r.specification = ListOfVariable{v};
	const Bytes wire = Encode(ConfirmedRequest{4, r});
	const ConfirmedRequest back = std::get<ConfirmedRequest>(Decode(wire));
	REQUIRE(std::get<ListOfVariable>(Service<ReadRequest>(back).specification)[0].alternateAccess == v.alternateAccess);
	REQUIRE(Encode(back) == wire);
}

TEST_CASE(SUITE("Invoke id tracking"))
{
	InvokeIdTracker t(3, 10);
	REQUIRE(t.Allocate() == 10u);
	REQUIRE(t.Allocate() == 11u);
	REQUIRE(t.Allocate() == 12u);
	REQUIRE(t.Outstanding() == 3);
	REQUIRE_FALSE(t.Allocate().has_value()); //at the limit
	REQUIRE(t.IsOutstanding(11));
	REQUIRE(t.Complete(11));
	REQUIRE_FALSE(t.IsOutstanding(11));
	REQUIRE_FALSE(t.Complete(11)); //a duplicate response
	REQUIRE_FALSE(t.Complete(99)); //an unsolicited response
	REQUIRE(t.Allocate() == 13u);
	t.SetMaxOutstanding(1);
	REQUIRE_FALSE(t.Allocate().has_value());
	REQUIRE(t.MaxOutstanding() == 1);
	t.Clear();
	REQUIRE(t.Outstanding() == 0);
	REQUIRE(t.Allocate() == 14u);

	//ids wrap, skipping any still outstanding
	InvokeIdTracker w(4, 0xFFFFFFFEu);
	REQUIRE(w.Allocate() == 0xFFFFFFFEu);
	REQUIRE(w.Allocate() == 0xFFFFFFFFu);
	REQUIRE(w.Allocate() == 0u);
	REQUIRE(w.Complete(0xFFFFFFFEu));
	REQUIRE(w.Allocate() == 1u);

	//a limit of zero sends nothing
	InvokeIdTracker none(0);
	REQUIRE_FALSE(none.Allocate().has_value());

	//ids that stay outstanding are never handed out twice
	InvokeIdTracker busy(2, 1);
	REQUIRE(busy.Allocate() == 1u);
	REQUIRE(busy.Allocate() == 2u);
	REQUIRE(busy.Complete(2));
	REQUIRE(busy.Allocate() == 3u);
	REQUIRE(busy.Complete(1));
	REQUIRE(busy.Allocate() == 4u);
}

TEST_CASE(SUITE("Truncation never succeeds and never crashes"))
{
	for(const char* fixture : AllFixtures)
	{
		const Bytes wire = Hex(fixture);
		for(size_t n = 0; n < wire.size(); ++n)
		{
			const Bytes cut(wire.begin(), wire.begin() + n);
			REQUIRE_THROWS_AS(Decode(cut), iccp::ber::DecodeError);
		}
		REQUIRE_NOTHROW(Decode(wire));
	}
}

TEST_CASE(SUITE("Mutated PDUs only ever raise DecodeError, and what decodes can be encoded again"))
{
	std::vector<Bytes> originals;
	for(const char* fixture : AllFixtures)
		originals.push_back(Hex(fixture));

	uint32_t state = 2024;
	auto next = [&state]()
			{
				state = state * 1664525u + 1013904223u;
				return state >> 8;
			};

	size_t decoded = 0;
	for(int iter = 0; iter < 60000; ++iter)
	{
		Bytes m = originals[next() % originals.size()];
		const unsigned changes = 1 + next() % 3;
		for(unsigned c = 0; c < changes; ++c)
			m[next() % m.size()] = static_cast<uint8_t>(next());
		switch(next() % 6)
		{
			case 0: m.resize(next() % (m.size() + 1)); break;
			case 1: m.insert(m.begin() + next() % (m.size() + 1), static_cast<uint8_t>(next())); break;
			default: break;
		}
		try
		{
			const Pdu p = Decode(m);
			++decoded;
			const Bytes again = Encode(p);
			REQUIRE(Decode(again) == p);
		}
		catch(const iccp::ber::DecodeError&)
		{}
	}
	REQUIRE(decoded > 0);
}

//Optional regression against externally supplied data: ICCP_MMS_CORPUS names a text file with one MMS PDU
//per line as hex. Every PDU must decode and re-encode byte-identically. Nothing from the file is printed
//on failure, only the line number.
TEST_CASE(SUITE("MMS PDUs from a corpus file (optional)"))
{
	std::string path;
	#ifdef _MSC_VER
	char* env = nullptr;
	size_t envLen = 0;
	if(_dupenv_s(&env, &envLen, "ICCP_MMS_CORPUS") == 0 && env != nullptr)
	{
		path = env;
		free(env);
	}
	#else
	if(const char* env = std::getenv("ICCP_MMS_CORPUS"))
		path = env;
	#endif
	if(path.empty())
		return;

	std::ifstream in(path);
	REQUIRE(in.good());
	std::string line;
	size_t lineNo = 0, ok = 0, undecodable = 0, notIdentical = 0, largest = 0;
	while(std::getline(in, line))
	{
		++lineNo;
		const Bytes wire = Hex(line);
		if(wire.empty())
			continue;
		largest = std::max(largest, wire.size());
		try
		{
			const Pdu p = Decode(wire);
			if(Encode(p) == wire)
				++ok;
			else
				++notIdentical;
		}
		catch(const std::exception&)
		{
			++undecodable;
		}
	}
	INFO("lines " << lineNo << ", identical " << ok << ", not identical " << notIdentical << ", undecodable " << undecodable << ", largest " << largest);
	REQUIRE(ok > 0);
	REQUIRE(undecodable == 0);
	REQUIRE(notIdentical == 0);
}
