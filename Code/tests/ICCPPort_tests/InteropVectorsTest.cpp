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
 * InteropVectorsTest.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Interoperability vectors for the OSI stack: TPKT -> COTP -> Session -> Presentation -> ACSE.
 *
 *  The frames are synthetic: they were assembled with an independent encoder from invented values (all
 *  names, addresses, selectors and titles here are made up), but they keep the exact byte structure
 *  of association set-ups seen in the field - parameter order, optional fields present or absent,
 *  PI/PGI layout, tag and length forms.
 *
 *  Each frame is decoded layer by layer, then re-encoded (both from the decoded structures and from values
 *  built from scratch) and compared byte for byte.
 *
 *  Structure notes:
 *   - COTP: class 0, TPDU size 1024 (parameter order c1 c2 c0 in both CR and CC)
 *   - Session CN: PGI 5 {PI 19 = 0, PI 22 = 2}, PI 20 = duplex, PI 51/52 selectors, PI 193 user data
 *   - Session AC: PGI 5 {PI 19 = 0, PI 22 = 2}, PI 20 = duplex, PI 193 user data; no selectors
 *   - Presentation: normal mode, 4 octet selectors, contexts 1 (ACSE) and 3 (MMS), BER, requirements 00
 *   - ACSE: protocol-version always written, called/calling AP-title form 2 OID, AE-qualifier form 2,
 *           user-information EXTERNAL with only an indirect-reference (the MMS context, 3)
 */

#include "Acse.h"
#include "COTP.h"
#include "Presentation.h"
#include "Session.h"
#include "TPKT.h"
#include <algorithm>
#include <catch.hpp>
#include <cstdint>
#include <string>
#include <vector>

#define SUITE(name) "ICCP interop vectors - " name

namespace
{
using iccp::ber::Bytes;

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

Bytes View(const uint8_t* p, size_t n)
{
	return n == 0 ? Bytes() : Bytes(p, p + n);
}

//---------------------------------------------------------------------------------------------
// Association 1 and 2: synthetic identities (TSAPs 07d1/07d3 -> 07d2, session selectors a1b1/a1b3 -> a2b2,
// presentation selectors 0000a301/0000a303 -> 0000a302)
//---------------------------------------------------------------------------------------------
const char* A1_CR = "03000016 11e000004c3d00c10207d1c20207d2c0010a";
const char* A1_CC = "03000016 11d04c3d5e6f00c10207d1c20207d2c0010a";
const char* A1_CN =
	"030000cd 02f080 0dc4 0506 130100 160102 1402 0002 3302a1b1 3402a2b2 c1ae"
	"3181aba003800101a281a381040000a30182040000a302a423300f0201010604520100013004060251013010020103060528ca22020130040602510188020600"
	"616c306a020101a065606380020780a107060528ca220203a20b06092bce0f010203040506a3040202012da60b06092bce0f010203040507a7040202008f"
	"be2e282c020103a027a82580024e2081010a82010a830103a416800101810305f800820c035c1c000000000004000190";
const char* A1_AC =
	"030000a7 02f080 0e9e 0506 130100 160102 1402 0002 c190"
	"31818da003800101a28185830400 00a302a5123007800100810251013007800100810251018802060061"
	"653063020101a05e615c80020780a107060528ca220203a203020100a305a103020100a40b06092bce0f010203040506a5040202012d"
	"be2e282c020103a027a92580024e20810104820104830103a416800101810305e000820c035c18000000000000000110";

const char* A2_CR = "03000016 11e000001b2c00c10207d3c20207d2c0010a";
const char* A2_CC = "03000016 11d01b2c3d4e00c10207d3c20207d2c0010a";
const char* A2_CN =
	"030000cd 02f080 0dc4 0506 130100 160102 1402 0002 3302a1b3 3402a2b2 c1ae"
	"3181aba003800101a281a381040000a30382040000a302a423300f0201010604520100013004060251013010020103060528ca22020130040602510188020600"
	"616c306a020101a065606380020780a107060528ca220203a20b06092bce0f010203040506a3040202012da60b06092bce0f010203040508a704020200 91"
	"be2e282c020103a027a82580024e2081010a82010a830103a416800101810305f800820c035c1c000000000004000190";
const char* A2_AC = A1_AC;

//MMS Initiate-RequestPDU / Initiate-ResponsePDU, decoded by hand (ISO 9506-2). Values are synthetic.
//
//  a8 25                                initiate-RequestPDU [8]
//     80 02 4e 20                         localDetailCalling [0]                       20000
//     81 01 0a                            proposedMaxServOutstandingCalling [1]        10
//     82 01 0a                            proposedMaxServOutstandingCalled [2]         10
//     83 01 03                            proposedDataStructureNestingLevel [3]        3
//     a4 16                               initRequestDetail [4]
//        80 01 01                           proposedVersionNumber [0]                  1
//        81 03 05 f8 00                     proposedParameterCBB [1]  11 bits 11111000000
//        82 0c 03 5c 1c 00 00 00 00 00 04 00 01 90
//                                           servicesSupportedCalling [2]  85 bits; set bits:
//                                             1 3 4 5 11 12 13 61 79 80 83
//
//  a9 25                                initiate-ResponsePDU [9]
//     80 02 4e 20                         localDetailCalled [0]                        20000
//     81 01 04                            negotiatedMaxServOutstandingCalling [1]      4
//     82 01 04                            negotiatedMaxServOutstandingCalled [2]       4
//     83 01 03                            negotiatedDataStructureNestingLevel [3]      3
//     a4 16                               initResponseDetail [4]
//        80 01 01                           negotiatedVersionNumber [0]                1
//        81 03 05 e0 00                     negotiatedParameterCBB [1]
//        82 0c 03 5c 18 00 00 00 00 00 00 00 01 10
//                                           servicesSupportedCalled [2]  set bits: 1 3 4 5 11 12 79 83
const char* MmsInitiateRequest =
	"a825 80024e20 81010a 82010a 830103 a416 800101 810305f800 820c035c1c000000000004000190";
const char* MmsInitiateResponse =
	"a925 80024e20 810104 820104 830103 a416 800101 810305e000 820c035c18000000000000000110";

//Data phase frames: GT+DT, presentation context 3 (MMS), MMS confirmed PDUs
const char* DataIdentifyRequest = //invoke 0x1234
                                  "0300001c 02f080 01000100 610f300d020103a008a006020212348200";
const char* DataIdentifyResponse = //vendor "VENDOR_X", model "MODEL_Y", revision "REV_Z"
                                   "03000036 02f080 01000100 61293027020103a022a12002021234a21a800856454e444f525f5881074d4f44454c5f5982055245565f5a";
const char* DataReadRequest = //three variables: one domain-specific, two VMD-specific
                              "03000059 02f080 01000100 614c304a020103a045"
                              "a04302022345a43da13ba0393019a017a1151a08444f4d41494e5f411a094954454d5f30303031300da00b80094954454d5f30303032300da00b80094954454d5f30303033";
const char* DataReadResponse = //visible-string, bit-string, structure of two integers
                               "03000035 02f080 01000100 61283026020103a021a11f02022345a419a1178a0756414c55455f31840304a000a207850207d1850107";

struct AssociationFrames
{
	const char* cr;
	const char* cc;
	const char* cn;
	const char* ac;
	uint16_t crSrcRef;
	uint16_t ccSrcRef;
	Bytes callingTsap;
	Bytes callingSessionSelector;
	Bytes callingPresentationSelector;
	iccp::acse::Oid callingAp;
	int64_t callingAe;
};

const iccp::acse::Oid CalledAp{1, 3, 9999, 1, 2, 3, 4, 5, 6};
constexpr int64_t CalledAe = 301;

const AssociationFrames Association1{A1_CR, A1_CC, A1_CN, A1_AC, 0x4c3d, 0x5e6f, {0x07, 0xd1}, {0xa1, 0xb1}, {0x00, 0x00, 0xa3, 0x01},
						 {1, 3, 9999, 1, 2, 3, 4, 5, 7}, 143};
const AssociationFrames Association2{A2_CR, A2_CC, A2_CN, A2_AC, 0x1b2c, 0x3d4e, {0x07, 0xd3}, {0xa1, 0xb3}, {0x00, 0x00, 0xa3, 0x03},
						 {1, 3, 9999, 1, 2, 3, 4, 5, 8}, 145};

//TPKT payload of the single TPKT in `wire`
Bytes Unframe(const Bytes& wire)
{
	iccp::tpkt::Reader reader;
	reader.Feed(wire);
	Bytes payload;
	REQUIRE(reader.Next(payload));
	REQUIRE(reader.Buffered() == 0);
	return payload;
}

//The TSDU carried by the single DT in a TPKT payload
Bytes TsduOf(const Bytes& tpduBytes)
{
	const auto tpdu = iccp::cotp::Decode(tpduBytes);
	REQUIRE(tpdu.type == iccp::cotp::TpduType::Data);
	REQUIRE(tpdu.eot);
	return View(tpdu.data, tpdu.dataLength);
}

Bytes WrapTsdu(const Bytes& tsdu)
{
	const auto segments = iccp::cotp::Segment(tsdu, iccp::cotp::TpduSize1024);
	REQUIRE(segments.size() == 1);
	return iccp::tpkt::Frame(segments[0]);
}

void CheckAssociationRequest(const AssociationFrames& f)
{
	using namespace iccp;
	const Bytes wire = Hex(f.cn);
	const Bytes tsdu = TsduOf(Unframe(wire));

	//--- session CONNECT
	const session::Spdu spdu = session::Decode(tsdu);
	REQUIRE(spdu.type == session::SpduType::Connect);
	REQUIRE(spdu.protocolOptions == 0x00);
	REQUIRE(spdu.version == session::Version::Version2);
	REQUIRE(spdu.requirements == session::Requirement::Duplex);
	REQUIRE(spdu.callingSelector == f.callingSessionSelector);
	REQUIRE(spdu.calledSelector == Bytes({0xa2, 0xb2}));
	REQUIRE_FALSE(spdu.tsduMaxSize.has_value());
	REQUIRE_FALSE(spdu.tokenSetting.has_value());
	REQUIRE(spdu.hasUserData);
	REQUIRE(spdu.userDataLength == 174);
	REQUIRE(session::Encode(spdu) == tsdu);

	//--- presentation CP
	const Bytes ppdu = View(spdu.userData, spdu.userDataLength);
	const presentation::ConnectRequest cp = presentation::DecodeConnectRequest(ppdu);
	REQUIRE(cp.callingSelector == f.callingPresentationSelector);
	REQUIRE(cp.calledSelector == Bytes({0x00, 0x00, 0xa3, 0x02}));
	REQUIRE(cp.contexts.size() == 2);
	REQUIRE(cp.contexts[0].id == 1);
	REQUIRE(cp.contexts[0].abstractSyntax == presentation::AcseAbstractSyntax);
	REQUIRE(cp.contexts[0].transferSyntaxes == std::vector<presentation::Oid>{presentation::BerTransferSyntax});
	REQUIRE(cp.contexts[1].id == 3);
	REQUIRE(cp.contexts[1].abstractSyntax == presentation::MmsAbstractSyntax);
	REQUIRE(cp.contexts[1].transferSyntaxes == std::vector<presentation::Oid>{presentation::BerTransferSyntax});
	REQUIRE_FALSE(cp.defaultContextName.has_value());
	REQUIRE(cp.presentationRequirements == 0x00);
	REQUIRE_FALSE(cp.userSessionRequirements.has_value());
	REQUIRE(cp.userData.size() == 1);
	REQUIRE(cp.userData[0].contextId == 1);
	REQUIRE(cp.userData[0].kind == presentation::PdvKind::SingleAsn1Type);
	REQUIRE(presentation::Encode(cp) == ppdu);

	//--- ACSE AARQ
	const Bytes apdu = View(cp.userData[0].data, cp.userData[0].dataLength);
	REQUIRE(acse::PeekApduType(apdu) == acse::ApduType::Aarq);
	const acse::Aarq aarq = acse::DecodeAarq(apdu);
	REQUIRE(aarq.protocolVersion == acse::ProtocolVersion::Version1);
	REQUIRE(aarq.encodeProtocolVersion);
	REQUIRE(aarq.applicationContextName == acse::MmsApplicationContext);
	REQUIRE(aarq.calledApTitle->form2);
	REQUIRE(aarq.calledApTitle->oid == CalledAp);
	REQUIRE(aarq.calledAeQualifier->form2);
	REQUIRE(aarq.calledAeQualifier->number == CalledAe);
	REQUIRE(aarq.callingApTitle->oid == f.callingAp);
	REQUIRE(aarq.callingAeQualifier->number == f.callingAe);
	REQUIRE_FALSE(aarq.calledApInvocationId.has_value());
	REQUIRE_FALSE(aarq.authentication.acseRequirements.has_value());
	REQUIRE_FALSE(aarq.authentication.mechanismName.has_value());
	REQUIRE_FALSE(aarq.authentication.authenticationValue.has_value());
	REQUIRE_FALSE(aarq.implementationInformation.has_value());
	REQUIRE(aarq.userInformation.size() == 1);
	const acse::External& x = aarq.userInformation[0];
	REQUIRE_FALSE(x.directReference.has_value());
	REQUIRE(x.indirectReference == 3);
	REQUIRE(x.encoding == acse::ExternalEncoding::SingleAsn1Type);
	REQUIRE(View(x.data, x.dataLength) == Hex(MmsInitiateRequest));
	REQUIRE(acse::Encode(aarq) == apdu);

	//--- from scratch, bottom up, to the same TPKT
	acse::Aarq a;
	a.encodeProtocolVersion = true;
	a.calledApTitle = acse::ApTitle{true, CalledAp, {}
	};
	a.calledAeQualifier = acse::AeQualifier{true, CalledAe, {}
	};
	a.callingApTitle = acse::ApTitle{true, f.callingAp, {}
	};
	a.callingAeQualifier = acse::AeQualifier{true, f.callingAe, {}
	};
	const Bytes initiate = Hex(MmsInitiateRequest);
	a.userInformation.push_back(acse::MakeExternal(3, initiate));
	const Bytes aarqBytes = acse::Encode(a);
	REQUIRE(aarqBytes == apdu);

	presentation::ConnectRequest c;
	c.callingSelector = f.callingPresentationSelector;
	c.calledSelector = Bytes{0x00, 0x00, 0xa3, 0x02};
	c.contexts.push_back({1, presentation::AcseAbstractSyntax, {presentation::BerTransferSyntax}
		});
	c.contexts.push_back({3, presentation::MmsAbstractSyntax, {presentation::BerTransferSyntax}
		});
	c.presentationRequirements = 0;
	c.userData.push_back(presentation::MakePdv(1, aarqBytes));
	const Bytes cpBytes = presentation::Encode(c);
	REQUIRE(cpBytes == ppdu);

	session::Spdu s;
	s.type = session::SpduType::Connect;
	s.protocolOptions = 0;
	s.version = session::Version::Version2;
	s.requirements = session::Requirement::Duplex;
	s.callingSelector = f.callingSessionSelector;
	s.calledSelector = Bytes{0xa2, 0xb2};
	s.hasUserData = true;
	s.userData = cpBytes.data();
	s.userDataLength = cpBytes.size();
	const Bytes snBytes = session::Encode(s);
	REQUIRE(snBytes == tsdu);
	REQUIRE(WrapTsdu(snBytes) == wire);
}

void CheckAssociationResponse(const AssociationFrames& f)
{
	using namespace iccp;
	const Bytes wire = Hex(f.ac);
	const Bytes tsdu = TsduOf(Unframe(wire));

	//--- session ACCEPT
	const session::Spdu spdu = session::Decode(tsdu);
	REQUIRE(spdu.type == session::SpduType::Accept);
	REQUIRE(spdu.protocolOptions == 0x00);
	REQUIRE(spdu.version == session::Version::Version2);
	REQUIRE(spdu.requirements == session::Requirement::Duplex);
	REQUIRE_FALSE(spdu.callingSelector.has_value());
	REQUIRE_FALSE(spdu.calledSelector.has_value());
	REQUIRE_FALSE(spdu.tsduMaxSize.has_value());
	REQUIRE(spdu.hasUserData);
	REQUIRE(spdu.userDataLength == 144);
	REQUIRE(session::Encode(spdu) == tsdu);

	//--- presentation CPA
	const Bytes ppdu = View(spdu.userData, spdu.userDataLength);
	const presentation::ConnectAccept cpa = presentation::DecodeConnectAccept(ppdu);
	REQUIRE(cpa.respondingSelector == Bytes({0x00, 0x00, 0xa3, 0x02}));
	REQUIRE(cpa.results.size() == 2);
	for(const auto& r : cpa.results)
	{
		REQUIRE(r.result == presentation::Result::Acceptance);
		REQUIRE(r.transferSyntax == presentation::BerTransferSyntax);
		REQUIRE_FALSE(r.providerReason.has_value());
	}
	REQUIRE(cpa.presentationRequirements == 0x00);
	REQUIRE_FALSE(cpa.userSessionRequirements.has_value());
	REQUIRE(cpa.userData.size() == 1);
	REQUIRE(cpa.userData[0].contextId == 1);
	REQUIRE(presentation::Encode(cpa) == ppdu);

	//--- ACSE AARE
	const Bytes apdu = View(cpa.userData[0].data, cpa.userData[0].dataLength);
	REQUIRE(acse::PeekApduType(apdu) == acse::ApduType::Aare);
	const acse::Aare aare = acse::DecodeAare(apdu);
	REQUIRE(aare.protocolVersion == acse::ProtocolVersion::Version1);
	REQUIRE(aare.encodeProtocolVersion);
	REQUIRE(aare.applicationContextName == acse::MmsApplicationContext);
	REQUIRE(aare.result == acse::Result::Accepted);
	REQUIRE_FALSE(aare.diagnostic.fromProvider);
	REQUIRE(aare.diagnostic.value == acse::UserDiagnostic::Null);
	REQUIRE(aare.respondingApTitle->form2);
	REQUIRE(aare.respondingApTitle->oid == CalledAp);
	REQUIRE(aare.respondingAeQualifier->form2);
	REQUIRE(aare.respondingAeQualifier->number == CalledAe);
	REQUIRE_FALSE(aare.authentication.acseRequirements.has_value());
	REQUIRE_FALSE(aare.implementationInformation.has_value());
	REQUIRE(aare.userInformation.size() == 1);
	const acse::External& x = aare.userInformation[0];
	REQUIRE_FALSE(x.directReference.has_value());
	REQUIRE(x.indirectReference == 3);
	REQUIRE(View(x.data, x.dataLength) == Hex(MmsInitiateResponse));
	REQUIRE(acse::Encode(aare) == apdu);

	//--- from scratch
	acse::Aare a;
	a.encodeProtocolVersion = true;
	a.result = acse::Result::Accepted;
	a.diagnostic = acse::SourceDiagnostic{false, acse::UserDiagnostic::Null};
	a.respondingApTitle = acse::ApTitle{true, CalledAp, {}
	};
	a.respondingAeQualifier = acse::AeQualifier{true, CalledAe, {}
	};
	const Bytes initiate = Hex(MmsInitiateResponse);
	a.userInformation.push_back(acse::MakeExternal(3, initiate));
	const Bytes aareBytes = acse::Encode(a);
	REQUIRE(aareBytes == apdu);

	presentation::ConnectAccept c;
	c.respondingSelector = Bytes{0x00, 0x00, 0xa3, 0x02};
	c.results.push_back({presentation::Result::Acceptance, presentation::BerTransferSyntax, std::nullopt});
	c.results.push_back({presentation::Result::Acceptance, presentation::BerTransferSyntax, std::nullopt});
	c.presentationRequirements = 0;
	c.userData.push_back(presentation::MakePdv(1, aareBytes));
	const Bytes cpaBytes = presentation::Encode(c);
	REQUIRE(cpaBytes == ppdu);

	session::Spdu s;
	s.type = session::SpduType::Accept;
	s.protocolOptions = 0;
	s.version = session::Version::Version2;
	s.requirements = session::Requirement::Duplex;
	s.hasUserData = true;
	s.userData = cpaBytes.data();
	s.userDataLength = cpaBytes.size();
	const Bytes acBytes = session::Encode(s);
	REQUIRE(acBytes == tsdu);
	REQUIRE(WrapTsdu(acBytes) == wire);
}

void CheckTransportConnect(const AssociationFrames& f)
{
	using namespace iccp;
	const Bytes called = {0x07, 0xd2};
	//CR, initiator -> responder
	const Bytes crWire = Hex(f.cr);
	const cotp::Tpdu cr = cotp::Decode(Unframe(crWire));
	REQUIRE(cr.type == cotp::TpduType::ConnectRequest);
	REQUIRE(cr.dstRef == 0);
	REQUIRE(cr.srcRef == f.crSrcRef);
	REQUIRE(cr.callingTsap == f.callingTsap);
	REQUIRE(cr.calledTsap == called);
	REQUIRE(cr.tpduSizeCode == cotp::TpduSize1024);
	REQUIRE(tpkt::Frame(cotp::Encode(cr)) == crWire);
	REQUIRE(tpkt::Frame(cotp::EncodeConnectRequest(f.crSrcRef, f.callingTsap, called, cotp::TpduSize1024)) == crWire);

	//CC, responder -> initiator: echoes both TSAPs and the size
	const Bytes ccWire = Hex(f.cc);
	const cotp::Tpdu cc = cotp::Decode(Unframe(ccWire));
	REQUIRE(cc.type == cotp::TpduType::ConnectConfirm);
	REQUIRE(cc.dstRef == f.crSrcRef);
	REQUIRE(cc.srcRef == f.ccSrcRef);
	REQUIRE(cc.callingTsap == f.callingTsap);
	REQUIRE(cc.calledTsap == called);
	REQUIRE(cc.tpduSizeCode == cotp::TpduSize1024);
	REQUIRE(tpkt::Frame(cotp::Encode(cc)) == ccWire);
	REQUIRE(tpkt::Frame(cotp::EncodeConnectConfirm(f.crSrcRef, f.ccSrcRef, f.callingTsap, called, cotp::TpduSize1024)) == ccWire);

	//what an initiator and a responder would negotiate
	REQUIRE(cotp::NegotiateTpduSize(cr.tpduSizeCode, cotp::TpduSize8192) == cotp::TpduSize1024);
	REQUIRE(cotp::ConfirmTpduSize(cr.tpduSizeCode, cc.tpduSizeCode) == cotp::TpduSize1024);
}

//Decode, then re-encode, one data phase frame; returns the MMS PDU
Bytes CheckDataFrame(const char* frame, size_t expectedMmsLength)
{
	using namespace iccp;
	const Bytes wire = Hex(frame);
	const Bytes tsdu = TsduOf(Unframe(wire));
	const session::Spdu spdu = session::Decode(tsdu);
	REQUIRE(spdu.type == session::SpduType::Data);
	const Bytes ppdu = View(spdu.userData, spdu.userDataLength);
	const presentation::Pdv pdv = presentation::DecodeData(ppdu);
	REQUIRE(pdv.contextId == 3); //MMS
	REQUIRE(pdv.kind == presentation::PdvKind::SingleAsn1Type);
	const Bytes mms = View(pdv.data, pdv.dataLength);
	REQUIRE(mms.size() == expectedMmsLength);

	REQUIRE(presentation::EncodeData(3, mms) == ppdu);
	REQUIRE(session::EncodeData(ppdu) == tsdu);
	REQUIRE(WrapTsdu(tsdu) == wire);
	return mms;
}
}

TEST_CASE(SUITE("COTP CR and CC, association 1"))
{
	CheckTransportConnect(Association1);
}

TEST_CASE(SUITE("COTP CR and CC, association 2"))
{
	CheckTransportConnect(Association2);
}

TEST_CASE(SUITE("Session CN / presentation CP / ACSE AARQ, association 1"))
{
	CheckAssociationRequest(Association1);
}

TEST_CASE(SUITE("Session CN / presentation CP / ACSE AARQ, association 2"))
{
	CheckAssociationRequest(Association2);
}

TEST_CASE(SUITE("Session AC / presentation CPA / ACSE AARE, association 1"))
{
	CheckAssociationResponse(Association1);
}

TEST_CASE(SUITE("Session AC / presentation CPA / ACSE AARE, association 2"))
{
	CheckAssociationResponse(Association2);
}

TEST_CASE(SUITE("Data phase frames: MMS PDUs after association"))
{
	//Identify request (confirmed-RequestPDU a0, invoke 0x1234, identify [2])
	REQUIRE(CheckDataFrame(DataIdentifyRequest, 8) == Hex("a0 06 02 02 12 34 82 00"));
	//Identify response (confirmed-ResponsePDU a1): vendor, model, revision
	REQUIRE(CheckDataFrame(DataIdentifyResponse, 34) ==
		Hex("a1 20 02 02 12 34 a2 1a 80 08 56454e444f525f58 81 07 4d4f44454c5f59 82 05 5245565f5a"));
	//Read request: three variables, one domain-specific and two VMD-specific
	const Bytes read = CheckDataFrame(DataReadRequest, 69);
	REQUIRE(read[0] == 0xa0);
	REQUIRE(read[2] == 0x02);
	REQUIRE(read[4] == 0x23);
	//Read response: visible-string, bit-string, structure of two integers
	const Bytes resp = CheckDataFrame(DataReadResponse, 33);
	REQUIRE(resp == Hex("a1 1f 02 02 23 45 a4 19 a1 17 8a 07 56414c55455f31 84 03 04 a0 00 a2 07 85 02 07 d1 85 01 07"));
}

TEST_CASE(SUITE("Segmentation of a TSDU over the 1024 octet TPDU size"))
{
	//TSDUs larger than the TPDU are split into DTs of TPKT length 0x0404 (4 + 3 + 1021), the last with EOT:
	//a 2103 octet session SPDU becomes TPKTs of 1028, 1028 and 68 octets
	Bytes tsdu(2103);
	for(size_t i = 0; i < tsdu.size(); ++i)
		tsdu[i] = static_cast<uint8_t>(i * 13 + 1);
	const auto segments = iccp::cotp::Segment(tsdu, iccp::cotp::TpduSize1024);
	REQUIRE(segments.size() == 3);
	std::vector<Bytes> wire;
	for(const auto& s : segments)
		wire.push_back(iccp::tpkt::Frame(s));
	REQUIRE(wire[0].size() == 1028);
	REQUIRE(wire[1].size() == 1028);
	REQUIRE(wire[2].size() == 68);
	REQUIRE(Bytes(wire[0].begin(), wire[0].begin() + 7) == Hex("03 00 04 04 02 f0 00"));
	REQUIRE(Bytes(wire[1].begin(), wire[1].begin() + 7) == Hex("03 00 04 04 02 f0 00"));
	REQUIRE(Bytes(wire[2].begin(), wire[2].begin() + 7) == Hex("03 00 00 44 02 f0 80"));

	//and reassembly over an arbitrarily chunked stream
	iccp::tpkt::Reader reader;
	iccp::cotp::TsduReassembler reassembler(iccp::cotp::TsduReassembler::DefaultMaxTsdu, iccp::cotp::TpduSize1024);
	Bytes stream;
	for(const auto& w : wire)
		stream.insert(stream.end(), w.begin(), w.end());
	bool done = false;
	for(size_t i = 0; i < stream.size(); i += 333)
	{
		reader.Feed(stream.data() + i, std::min<size_t>(333, stream.size() - i));
		Bytes payload;
		while(reader.Next(payload))
		{
			const auto dt = iccp::cotp::Decode(payload);
			done = reassembler.Add(dt);
		}
	}
	REQUIRE(done);
	REQUIRE(reassembler.Take() == tsdu);
}
