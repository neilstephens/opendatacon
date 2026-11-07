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
 * Mms.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  ISO 9506-2 manufacturing message specification: the subset used by ICCP / TASE.2.
 *
 *  An MMS PDU travels as the single-ASN.1-type PDV of the MMS presentation context, so Decode()
 *  takes the PDV data and Encode() returns the bytes for it.
 *
 *  PDUs:       confirmed-Request / -Response / -Error, unconfirmed (InformationReport only), Reject,
 *              Conclude request / response / error, Initiate request / response / error
 *  Services:   Identify, GetNameList, Read, Write, GetVariableAccessAttributes, DefineNamedVariableList,
 *              GetNamedVariableListAttributes, DeleteNamedVariableList
 *  Not supported (DecodeError): listOfModifier, Cancel, other services, extended object classes, Address
 *              and scattered access variable specifications. PeekInvokeId() lets the caller answer an
 *              undecodable confirmed request with a Reject or an Error.
 *
 *  Data is held generically: scalars keep their content octets exactly as on the wire, so any PDU that
 *  decodes re-encodes byte-identically (non-minimal integers, BOOLEAN 0x01 vs 0xFF and so on are
 *  preserved) while typed accessors and factories give convenient access. Values built through the
 *  factories use minimal encodings, and BOOLEAN TRUE is written as 0x01 (as ICCP peers send it); other
 *  protocol booleans (mmsDeletable, moreFollows, ...) are written the same way.
 *
 *  This is a pure byte-buffer codec, with no I/O. Decoding is defensive: every read is bounds checked
 *  and any malformed input results in a DecodeError (the input is untrusted network data), with caps on
 *  nesting, list sizes and the total number of Data nodes in a PDU. Optional trailing elements inside a
 *  SEQUENCE that are not understood (context-specific) are skipped.
 *
 *  Encoders throw std::invalid_argument if a PDU is not encodable.
 */

#ifndef ICCP_MMS_H_
#define ICCP_MMS_H_

#include "BER.h"
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace iccp
{
namespace mms
{

using ber::BitString;
using ber::Bytes;
using ber::DecodeError;
using Oid = std::vector<uint32_t>;

//Decoder limits
constexpr size_t MaxListEntries = 100000;   //identifiers, variables, data or results in one list
constexpr size_t MaxDataNodes = 200000;     //Data / TypeDescription nodes in one PDU
constexpr unsigned MaxNesting = 32;         //Data and TypeDescription nesting
constexpr size_t MaxIdentifierLength = 255; //octets in an Identifier

// ---------------------------------------------------------------------------------------------
// Object names
// ---------------------------------------------------------------------------------------------

enum class ObjectScope : uint8_t
{
	VmdSpecific = 0,
	DomainSpecific = 1,
	AaSpecific = 2
};

struct ObjectName
{
	ObjectScope scope = ObjectScope::VmdSpecific;
	std::string domainId; //DomainSpecific only
	std::string itemId;

	static ObjectName Vmd(std::string item)
	{
		return ObjectName{ObjectScope::VmdSpecific, {}, std::move(item)};
	}
	static ObjectName Domain(std::string domain, std::string item)
	{
		return ObjectName{ObjectScope::DomainSpecific, std::move(domain), std::move(item)};
	}
	static ObjectName Aa(std::string item)
	{
		return ObjectName{ObjectScope::AaSpecific, {}, std::move(item)};
	}
	bool operator==(const ObjectName&) const = default;
};

// ---------------------------------------------------------------------------------------------
// Time values
// ---------------------------------------------------------------------------------------------

//UtcTime: 4 octets seconds since 1970-01-01, 3 octets fraction of a second (units of 1/2^24 s), 1 octet quality
struct UtcTime
{
	uint32_t seconds = 0;
	uint32_t fraction = 0; //24 bits
	uint8_t quality = 0;
	bool operator==(const UtcTime&) const = default;
};

//BinaryTime (TimeOfDay): milliseconds since midnight, and optionally days since 1984-01-01
struct BinaryTime
{
	uint32_t milliseconds = 0;
	std::optional<uint16_t> days;
	bool operator==(const BinaryTime&) const = default;
};

// ---------------------------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------------------------

//Values are the context tag numbers of the Data CHOICE
enum class DataType : uint8_t
{
	Array = 1,
	Structure = 2,
	Boolean = 3,
	BitString = 4,
	Integer = 5,
	Unsigned = 6,
	FloatingPoint = 7,
	OctetString = 9,
	VisibleString = 10,
	GeneralizedTime = 11,
	BinaryTime = 12,
	Bcd = 13,
	BooleanArray = 14,
	ObjectId = 15,
	MmsString = 16,
	UtcTime = 17
};

struct Data
{
	DataType type = DataType::Structure;
	//scalars: the content octets as on the wire (BIT STRING / BooleanArray include the unused-bits octet)
	Bytes content;
	//Array and Structure
	std::vector<Data> elements;

	bool operator==(const Data&) const = default;

	//Factories (canonical encodings)
	static Data Boolean(bool value);
	static Data Integer(int64_t value);
	static Data Unsigned(uint64_t value);
	static Data Float32(float value);  //FloatingPoint, exponent width 8: 08 + IEEE 754 single
	static Data Float64(double value); //FloatingPoint, exponent width 11: 0b + IEEE 754 double
	static Data Bits(const BitString& value);
	static Data BitsArray(const BitString& value); //booleanArray
	static Data OctetString(const Bytes& value);
	static Data VisibleString(const std::string& value);
	static Data MmsString(const std::string& value);
	static Data GeneralizedTime(const std::string& value);
	static Data Bcd(uint64_t value);
	static Data ObjectId(const Oid& value);
	static Data Utc(const UtcTime& value);
	static Data Binary(const BinaryTime& value);
	static Data Structure(std::vector<Data> members);
	static Data Array(std::vector<Data> members);

	//Accessors throw DecodeError if the type or the content does not fit
	bool AsBoolean() const;
	int64_t AsInteger() const;
	uint64_t AsUnsigned() const;
	double AsFloat() const;   //exponent width 8 (32 bit) or 11 (64 bit) only
	BitString AsBits() const; //BitString and BooleanArray
	Bytes AsOctetString() const;
	std::string AsString() const; //VisibleString, MmsString, GeneralizedTime
	uint64_t AsBcd() const;
	Oid AsObjectId() const;
	UtcTime AsUtc() const;
	BinaryTime AsBinary() const;
};

// ---------------------------------------------------------------------------------------------
// Type description (GetVariableAccessAttributes)
// ---------------------------------------------------------------------------------------------

struct TypeDescription
{
	enum class Kind : uint8_t
	{
		TypeName = 0,
		Array = 1,
		Structure = 2,
		Boolean = 3,
		BitString = 4,
		Integer = 5,
		Unsigned = 6,
		FloatingPoint = 7,
		OctetString = 9,
		VisibleString = 10,
		GeneralizedTime = 11,
		BinaryTime = 12,
		Bcd = 13,
		ObjectId = 15,
		MmsString = 16,
		UtcTime = 17
	};
	Kind kind = Kind::Boolean;
	bool packed = false;                //Array, Structure
	int64_t size = 0;                   //BitString/OctetString/VisibleString/MmsString length; Integer/Unsigned/Bcd width;
	                                    //Array: numberOfElements
	bool timeOfDay = false;             //BinaryTime
	uint8_t formatWidth = 0;            //FloatingPoint
	uint8_t exponentWidth = 0;          //FloatingPoint
	std::optional<ObjectName> typeName; //TypeName
	//Array: one element type. Structure: the component types, with parallel (optional) component names.
	std::vector<TypeDescription> subtypes;
	std::vector<std::optional<std::string>> componentNames;

	bool operator==(const TypeDescription&) const = default;
};

// ---------------------------------------------------------------------------------------------
// Variable access
// ---------------------------------------------------------------------------------------------

//One variable of a listOfVariable. Only name specifications are supported; an AlternateAccess, if present,
//is carried as its complete BER element.
struct VariableSpec
{
	ObjectName name;
	std::optional<Bytes> alternateAccess;
	bool operator==(const VariableSpec&) const = default;
};

using ListOfVariable = std::vector<VariableSpec>;
//listOfVariable [0] or variableListName [1]
using VariableAccessSpecification = std::variant<ListOfVariable, ObjectName>;

//failure [0] DataAccessError, or success Data
struct AccessResult
{
	bool success = true;
	int64_t error = 0; //DataAccessError, when !success
	Data data;         //when success

	static AccessResult Success(Data d)
	{
		AccessResult r;
		r.success = true;
		r.data = std::move(d);
		return r;
	}
	static AccessResult Failure(int64_t dataAccessError)
	{
		AccessResult r;
		r.success = false;
		r.error = dataAccessError;
		return r;
	}
	bool operator==(const AccessResult&) const = default;
};

// ---------------------------------------------------------------------------------------------
// Service errors
// ---------------------------------------------------------------------------------------------

//ServiceError.errorClass choices (the context tag number of the alternative)
namespace ErrorClass
{
constexpr int64_t VmdState = 0;
constexpr int64_t ApplicationReference = 1;
constexpr int64_t Definition = 2;
constexpr int64_t Resource = 3;
constexpr int64_t Service = 4;
constexpr int64_t ServicePreempt = 5;
constexpr int64_t TimeResolution = 6;
constexpr int64_t Access = 7;
constexpr int64_t Initiate = 8;
constexpr int64_t Conclude = 9;
constexpr int64_t Cancel = 10;
constexpr int64_t File = 11;
constexpr int64_t Others = 12;
}

struct ServiceError
{
	int64_t errorClass = ErrorClass::Others;
	int64_t errorCode = 0;
	std::optional<int64_t> additionalCode;
	std::optional<std::string> additionalDescription;
	std::optional<Bytes> serviceSpecificInformation; //complete BER element, not interpreted
	bool operator==(const ServiceError&) const = default;
};

//DataAccessError values (AccessResult failure)
namespace DataAccessError
{
constexpr int64_t ObjectInvalidated = 0;
constexpr int64_t HardwareFault = 1;
constexpr int64_t TemporarilyUnavailable = 2;
constexpr int64_t ObjectAccessDenied = 3;
constexpr int64_t ObjectUndefined = 4;
constexpr int64_t InvalidAddress = 5;
constexpr int64_t TypeUnsupported = 6;
constexpr int64_t TypeInconsistent = 7;
constexpr int64_t ObjectAttributeInconsistent = 8;
constexpr int64_t ObjectAccessUnsupported = 9;
constexpr int64_t ObjectNonExistent = 10;
constexpr int64_t ObjectValueInvalid = 11;
}

// ---------------------------------------------------------------------------------------------
// Services
// ---------------------------------------------------------------------------------------------

//GetNameList basicObjectClass values
namespace ObjectClass
{
constexpr int64_t NamedVariable = 0;
constexpr int64_t ScatteredAccess = 1;
constexpr int64_t NamedVariableList = 2;
constexpr int64_t NamedType = 3;
constexpr int64_t Semaphore = 4;
constexpr int64_t EventCondition = 5;
constexpr int64_t EventAction = 6;
constexpr int64_t EventEnrollment = 7;
constexpr int64_t Journal = 8;
constexpr int64_t Domain = 9;
constexpr int64_t ProgramInvocation = 10;
constexpr int64_t OperatorStation = 11;
}

//DeleteNamedVariableList scopeOfDelete values
namespace DeleteScope
{
constexpr int64_t Specific = 0; //the default
constexpr int64_t AaSpecific = 1;
constexpr int64_t Domain = 2;
constexpr int64_t Vmd = 3;
}

struct IdentifyRequest
{
	bool operator==(const IdentifyRequest&) const = default;
};
struct IdentifyResponse
{
	std::string vendorName;
	std::string modelName;
	std::string revision;
	std::optional<std::vector<Oid>> listOfAbstractSyntaxes;
	bool operator==(const IdentifyResponse&) const = default;
};

struct GetNameListRequest
{
	int64_t objectClass = ObjectClass::NamedVariable; //basicObjectClass
	ObjectScope scope = ObjectScope::VmdSpecific;
	std::string domainId; //DomainSpecific scope
	std::optional<std::string> continueAfter;
	bool operator==(const GetNameListRequest&) const = default;
};
struct GetNameListResponse
{
	std::vector<std::string> identifiers;
	//moreFollows defaults to TRUE when absent; some peers omit it for TRUE, so it is kept as sent
	std::optional<bool> moreFollows;
	bool MoreFollows() const
	{
		return moreFollows.value_or(true);
	}
	bool operator==(const GetNameListResponse&) const = default;
};

struct ReadRequest
{
	bool specificationWithResult = false;
	VariableAccessSpecification specification;
	bool operator==(const ReadRequest&) const = default;
};
struct ReadResponse
{
	std::optional<VariableAccessSpecification> specification; //present only if specificationWithResult was requested
	std::vector<AccessResult> results;
	bool operator==(const ReadResponse&) const = default;
};

struct WriteRequest
{
	VariableAccessSpecification specification;
	std::vector<Data> data;
	bool operator==(const WriteRequest&) const = default;
};
//failure [0] DataAccessError, or success [1] NULL
struct WriteResult
{
	bool success = true;
	int64_t error = 0;
	bool operator==(const WriteResult&) const = default;
};
struct WriteResponse
{
	std::vector<WriteResult> results;
	bool operator==(const WriteResponse&) const = default;
};

struct GetVariableAccessAttributesRequest
{
	ObjectName name;
	bool operator==(const GetVariableAccessAttributesRequest&) const = default;
};
struct GetVariableAccessAttributesResponse
{
	bool mmsDeletable = false;
	TypeDescription typeDescription;
	std::optional<std::string> accessControlListName;
	bool operator==(const GetVariableAccessAttributesResponse&) const = default;
};

struct DefineNamedVariableListRequest
{
	ObjectName name;
	ListOfVariable variables;
	bool operator==(const DefineNamedVariableListRequest&) const = default;
};
struct DefineNamedVariableListResponse
{
	bool operator==(const DefineNamedVariableListResponse&) const = default;
};

struct GetNamedVariableListAttributesRequest
{
	ObjectName name;
	bool operator==(const GetNamedVariableListAttributesRequest&) const = default;
};
struct GetNamedVariableListAttributesResponse
{
	bool mmsDeletable = false;
	ListOfVariable variables;
	std::optional<std::string> accessControlListName;
	bool operator==(const GetNamedVariableListAttributesResponse&) const = default;
};

struct DeleteNamedVariableListRequest
{
	std::optional<int64_t> scopeOfDelete; //DeleteScope; absent means Specific
	std::optional<std::vector<ObjectName>> listOfVariableListName;
	std::optional<std::string> domainName;
	bool operator==(const DeleteNamedVariableListRequest&) const = default;
};
struct DeleteNamedVariableListResponse
{
	uint32_t numberMatched = 0;
	uint32_t numberDeleted = 0;
	bool operator==(const DeleteNamedVariableListResponse&) const = default;
};

using ConfirmedServiceRequest = std::variant<IdentifyRequest, GetNameListRequest, ReadRequest, WriteRequest,
	GetVariableAccessAttributesRequest, DefineNamedVariableListRequest, GetNamedVariableListAttributesRequest,
	DeleteNamedVariableListRequest>;
using ConfirmedServiceResponse = std::variant<IdentifyResponse, GetNameListResponse, ReadResponse, WriteResponse,
	GetVariableAccessAttributesResponse, DefineNamedVariableListResponse, GetNamedVariableListAttributesResponse,
	DeleteNamedVariableListResponse>;

// ---------------------------------------------------------------------------------------------
// PDUs
// ---------------------------------------------------------------------------------------------

struct ConfirmedRequest
{
	uint32_t invokeId = 0;
	ConfirmedServiceRequest service;
	bool operator==(const ConfirmedRequest&) const = default;
};
struct ConfirmedResponse
{
	uint32_t invokeId = 0;
	ConfirmedServiceResponse service;
	bool operator==(const ConfirmedResponse&) const = default;
};
struct ConfirmedError
{
	uint32_t invokeId = 0;
	std::optional<int64_t> modifierPosition;
	ServiceError error;
	bool operator==(const ConfirmedError&) const = default;
};

//unconfirmed-PDU holding an InformationReport
struct InformationReport
{
	VariableAccessSpecification specification;
	std::vector<AccessResult> results;
	bool operator==(const InformationReport&) const = default;
};

//Reject reason types: the context tag number of the rejectReason alternative
namespace RejectType
{
constexpr int64_t ConfirmedRequestPdu = 1;
constexpr int64_t ConfirmedResponsePdu = 2;
constexpr int64_t ConfirmedErrorPdu = 3;
constexpr int64_t UnconfirmedPdu = 4;
constexpr int64_t PduError = 5;
constexpr int64_t CancelRequestPdu = 6;
constexpr int64_t CancelResponsePdu = 7;
constexpr int64_t CancelErrorPdu = 8;
constexpr int64_t ConcludeRequestPdu = 9;
constexpr int64_t ConcludeResponsePdu = 10;
constexpr int64_t ConcludeErrorPdu = 11;
}
//a few common reason codes, for the confirmed-requestPDU reject type. The values are those of the ISO 9506-2 ASN.1
//(other 0, unrecognized-service 1, unrecognized-modifier 2, invalid-invokeID 3, invalid-argument 4, invalid-modifier 5,
//max-serv-outstanding-exceeded 6, max-recursion-exceeded 8, value-out-of-range 9), as remembered from the standard and
//Wireshark's MMS dissector: not checked against a capture of a real peer's Reject.
namespace RejectReason
{
constexpr int64_t UnrecognizedService = 1;
constexpr int64_t InvalidInvokeId = 3;
constexpr int64_t InvalidArgument = 4;
constexpr int64_t MaxServOutstandingExceeded = 6;
constexpr int64_t MaxRecursionExceeded = 8;
constexpr int64_t ValueOutOfRange = 9;
}

struct Reject
{
	std::optional<uint32_t> originalInvokeId;
	int64_t type = RejectType::ConfirmedRequestPdu;
	int64_t reason = 0;
	bool operator==(const Reject&) const = default;
};

struct ConcludeRequest
{
	bool operator==(const ConcludeRequest&) const = default;
};
struct ConcludeResponse
{
	bool operator==(const ConcludeResponse&) const = default;
};
struct ConcludeError
{
	ServiceError error;
	bool operator==(const ConcludeError&) const = default;
};

//ServiceSupportOptions bit numbers (ISO 9506-2) for the services in scope, and a few others
namespace ServiceBit
{
constexpr size_t Status = 0;
constexpr size_t GetNameList = 1;
constexpr size_t Identify = 2;
constexpr size_t Rename = 3;
constexpr size_t Read = 4;
constexpr size_t Write = 5;
constexpr size_t GetVariableAccessAttributes = 6;
constexpr size_t DefineNamedVariable = 7;
constexpr size_t DefineNamedVariableList = 11;
constexpr size_t GetNamedVariableListAttributes = 12;
constexpr size_t DeleteNamedVariableList = 13;
constexpr size_t DefineNamedType = 14;
constexpr size_t GetNamedTypeAttributes = 15;
constexpr size_t InformationReport = 79;
constexpr size_t Conclude = 83;
constexpr size_t Cancel = 84;
}
//ParameterSupportOptions bit numbers
namespace ParameterBit
{
constexpr size_t Str1 = 0;
constexpr size_t Str2 = 1;
constexpr size_t Vnam = 2;
constexpr size_t Valt = 3;
constexpr size_t Vadr = 4;
constexpr size_t Vsca = 5;
constexpr size_t Tpy = 6;
constexpr size_t Vlis = 7;
constexpr size_t Real = 8;
constexpr size_t Cei = 10;
}

//A BitString of totalBits with the given bit numbers set (bit 0 is the most significant bit of the first octet)
BitString MakeBits(std::initializer_list<size_t> setBits, size_t totalBits);

struct InitiateRequest
{
	std::optional<int64_t> localDetailCalling;
	int64_t proposedMaxServOutstandingCalling = 0;
	int64_t proposedMaxServOutstandingCalled = 0;
	std::optional<int64_t> proposedDataStructureNestingLevel;
	//initRequestDetail
	int64_t proposedVersionNumber = 1;
	BitString proposedParameterCbb;
	BitString servicesSupportedCalling;
	std::optional<BitString> additionalSupportedCalling;
	std::optional<BitString> additionalCbbSupportedCalling;
	std::optional<std::string> privilegeClassIdentityCalling;
	bool operator==(const InitiateRequest&) const = default;
};
struct InitiateResponse
{
	std::optional<int64_t> localDetailCalled;
	int64_t negotiatedMaxServOutstandingCalling = 0;
	int64_t negotiatedMaxServOutstandingCalled = 0;
	std::optional<int64_t> negotiatedDataStructureNestingLevel;
	//initResponseDetail
	int64_t negotiatedVersionNumber = 1;
	BitString negotiatedParameterCbb;
	BitString servicesSupportedCalled;
	std::optional<BitString> additionalSupportedCalled;
	std::optional<BitString> additionalCbbSupportedCalled;
	std::optional<std::string> privilegeClassIdentityCalled;
	bool operator==(const InitiateResponse&) const = default;
};
struct InitiateError
{
	ServiceError error;
	bool operator==(const InitiateError&) const = default;
};

using Pdu = std::variant<ConfirmedRequest, ConfirmedResponse, ConfirmedError, InformationReport, Reject,
	ConcludeRequest, ConcludeResponse, ConcludeError, InitiateRequest, InitiateResponse, InitiateError>;

//Decode a whole MMS PDU. Throws DecodeError on anything malformed or unsupported
//(trailing octets, wrong types, caps exceeded); std::invalid_argument for null data with a length.
Pdu Decode(const uint8_t* data, size_t length);
inline Pdu Decode(const Bytes& b)
{
	return Decode(b.data(), b.size());
}

//Encode a PDU. Throws std::invalid_argument if it is not encodable (invoke id or counts out of range, a bad
//reject type, a malformed alternateAccess or serviceSpecificInformation element, ...).
Bytes Encode(const Pdu& pdu);

//The invoke id of a confirmed-Request / -Response / -Error, read without fully decoding the PDU, so that an
//unsupported or malformed confirmed request can still be answered. Never throws; nullopt if it is not
//available. Reject also carries an optional original invoke id, which is returned too.
std::optional<uint32_t> PeekInvokeId(const uint8_t* data, size_t length);
inline std::optional<uint32_t> PeekInvokeId(const Bytes& b)
{
	return PeekInvokeId(b.data(), b.size());
}

//Data on its own (Write data lists, tests and the TASE.2 layer)
Data DecodeData(const uint8_t* data, size_t length);
inline Data DecodeData(const Bytes& b)
{
	return DecodeData(b.data(), b.size());
}
Bytes EncodeData(const Data& data);

// ---------------------------------------------------------------------------------------------
// Invoke id bookkeeping (no I/O, no timers, not thread safe)
// ---------------------------------------------------------------------------------------------

//Hands out invoke ids for our confirmed requests and enforces the negotiated limit on outstanding requests.
class InvokeIdTracker
{
public:
	//maxOutstanding: the negotiated limit (0 means no request may be sent)
	explicit InvokeIdTracker(size_t maxOutstanding = 1, uint32_t firstId = 1);

	//The next free invoke id, now outstanding. nullopt when at the limit.
	std::optional<uint32_t> Allocate();
	//A response, error or reject arrived: false if the id was not outstanding (ignore the PDU)
	bool Complete(uint32_t id);
	bool IsOutstanding(uint32_t id) const;
	size_t Outstanding() const
	{
		return outstanding.size();
	}
	size_t MaxOutstanding() const
	{
		return limit;
	}
	void SetMaxOutstanding(size_t maxOutstanding)
	{
		limit = maxOutstanding;
	}
	//forget everything outstanding (e.g. on association loss)
	void Clear();

private:
	std::set<uint32_t> outstanding;
	size_t limit;
	uint32_t next;
};

} // namespace mms
} // namespace iccp

#endif /* ICCP_MMS_H_ */
