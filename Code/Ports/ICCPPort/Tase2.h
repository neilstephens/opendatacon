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
 * Tase2.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  TASE.2 (ICCP, IEC 60870-6-503 / -702) Blocks 1 and 2 on top of the MMS codec (Mms.h). Block 3 (blocked transfers, the BlockData attribute) is in Tase2Blocked.h and
 *  Block 5 (device control) in Tase2Control.h.
 *
 *  Pure functions over iccp::mms types: no I/O, no timers, no state machines. Build* functions create MMS
 *  requests/responses/data; Parse* functions read them (throwing mms::DecodeError for anything that does not
 *  have the TASE.2 shape; the input is untrusted). Everything decoded can be rebuilt byte-identically.
 *
 *  Scoping: TASE.2 objects live in the VCC (the MMS VMD: vmd-specific names) or in an ICC (an MMS domain:
 *  domain-specific names). TASE2_Version and Supported_Features are VCC objects; Bilateral_Table_ID,
 *  Next_DSTransfer_Set, transfer sets, data values and data sets are normally ICC objects (VCC data values
 *  and data sets exist as well).
 *
 *  Provenance is marked per item: [V] = seen on the wire from real peers, [S] = from the specification
 *  as remembered and not yet confirmed against a peer. Where real peers differ from the specification that is
 *  noted as DEVIATION.
 */

#ifndef ICCP_TASE2_H_
#define ICCP_TASE2_H_

#include "Mms.h"
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace iccp
{
namespace tase2
{

using mms::Bytes;
using mms::Data;
using mms::DecodeError;
using mms::ObjectName;

// ---------------------------------------------------------------------------------------------
// Object names and scope
// ---------------------------------------------------------------------------------------------

namespace Name
{
//Association objects
inline constexpr const char* BilateralTableId = "Bilateral_Table_ID";  //ICC scope, VisibleString          [V]
inline constexpr const char* Tase2Version = "TASE2_Version";           //VCC scope, structure of 2 integers [V]
inline constexpr const char* SupportedFeatures = "Supported_Features"; //VCC scope, bit string              [V]
//Block 2 objects
inline constexpr const char* NextDsTransferSet = "Next_DSTransfer_Set";        //ICC scope               [V]
inline constexpr const char* TransferSetName = "Transfer_Set_Name";            //report member           [V]
inline constexpr const char* TransferSetTimeStamp = "Transfer_Set_Time_Stamp"; //report member           [S]
inline constexpr const char* DsConditionsDetected = "DSConditions_Detected";   //report member           [V]
inline constexpr const char* EventCodeDetected = "Event_Code_Detected";        //report member           [V]
//Critical reports (IEC 60870-6-503, Transfer_Report_ACK / Transfer_Report_NACK): the client's acknowledgement of a report         [T] names, [S] layout
inline constexpr const char* TransferReportAck = "Transfer_Report_ACK";
inline constexpr const char* TransferReportNack = "Transfer_Report_NACK";
}

//Bilateral_Table_ID is of the type Visible-String-32 (IEC 60870-6-503 clauses 8.2.2 and 8.3.3): a visible string of at most 32 characters
inline constexpr size_t BilateralTableIdWidth = 32;

//Scope as carried inside TASE.2 object-name structures (and by the MMS name form)
enum class Scope : uint8_t
{
	Vcc = 0, //vmd-specific
	Icc = 1  //domain-specific
};

inline ObjectName VccName(std::string item)
{
	return ObjectName::Vmd(std::move(item));
}
inline ObjectName IccName(std::string domain, std::string item)
{
	return ObjectName::Domain(std::move(domain), std::move(item));
}
//Throws DecodeError for aa-specific names, which TASE.2 does not use
Scope ScopeOf(const ObjectName& name);

//An object name as a TASE.2 data value (Transfer_Set_Name, Next_DSTransfer_Set, the data set name inside a
//DSTransferSet): STRUCTURE { Scope: unsigned, Domain: VisibleString, Name: VisibleString }.   [V] for Icc
//A Vcc reference has an empty domain. Decoding also accepts a two member {scope, name} form. [S]
struct ObjectRef
{
	Scope scope = Scope::Icc;
	std::string domain;
	std::string name;
	bool operator==(const ObjectRef&) const = default;

	static ObjectRef FromName(const ObjectName& n);
	ObjectName ToName() const;
};
Data ToData(const ObjectRef& ref);
ObjectRef ParseObjectRef(const Data& data);

// ---------------------------------------------------------------------------------------------
// Association objects
// ---------------------------------------------------------------------------------------------

//TASE2_Version: STRUCTURE { MajorVersionNumber: integer, MinorVersionNumber: integer }       [V]
//Both peers seen use the 1996-08 edition.
struct Version
{
	int64_t major = 0;
	int64_t minor = 0;
	bool operator==(const Version&) const = default;
};
inline constexpr Version Edition1996{1996, 8};
inline constexpr Version Edition2000{2000, 8};
Data ToData(const Version& v);
Version ParseVersion(const Data& data);

//Supported_Features: BIT STRING, bit i set when conformance block i+1 is supported (bit 0 is the most
//significant bit of the first octet).                                                          [S]
//Real peers send 12 bits ([V]); the specification names 9 blocks, bits 9..11 are not defined here.
enum class Block : uint8_t
{
	Block1 = 0, //basic services (data values, data sets, association)
	Block2 = 1, //extended conditions: data set transfer sets (DSTransfer)
	Block3 = 2, //blocked transfers [S]: advertised when 3 is in SupportedBlocks, which makes the server honour BlockData (Tase2Blocked.h); never seen from a real peer
	Block4 = 3, //operator stations
	Block5 = 4, //device control [S]: the objects and flows are in Tase2Control.h, all of it unverified against a real peer
	Block6 = 5, //programs
	Block7 = 6, //events
	Block8 = 7, //accounts
	Block9 = 8  //time series
};
constexpr size_t SupportedFeaturesBitCount = 12;
mms::BitString MakeSupportedFeatures(std::initializer_list<Block> blocks, size_t bitCount = SupportedFeaturesBitCount);
bool HasBlock(const mms::BitString& features, Block block);
Data ToData(const mms::BitString& features); //as a Data bit string
mms::BitString ParseSupportedFeatures(const Data& data);

//What a peer reports about itself; absent items were not asked for (or failed)
struct AssociationInfo
{
	std::optional<std::string> bilateralTableId;
	std::optional<Version> version;
	std::optional<mms::BitString> features;
	//data access errors for items that were requested but could not be read, by item name
	std::map<std::string, int64_t> failures;
	bool operator==(const AssociationInfo&) const = default;
};

//Read of Bilateral_Table_ID (in the given ICC domain), TASE2_Version and Supported_Features, in that order,
//as one MMS Read.                                                                               [V]
mms::ReadRequest BuildAssociationInfoRequest(const std::string& iccDomain, bool version = true, bool features = true);
//Match results to the request by position and item name. Requests for other items are ignored.
AssociationInfo ParseAssociationInfoResponse(const mms::ReadRequest& request, const mms::ReadResponse& response);

//What this side reports: one Bilateral_Table_ID per ICC domain it serves
struct LocalAssociation
{
	std::map<std::string, std::string> bilateralTableIds; //ICC domain -> id
	Version version = Edition1996;
	mms::BitString features = MakeSupportedFeatures({Block::Block1, Block::Block2});
};
//Answer a Read of association objects (in any order, any subset). Unknown objects or domains give the data
//access error object-non-existent; any other variable gives it too, so the caller can combine this with
//its data values by splitting the request.
mms::ReadResponse BuildAssociationInfoResponse(const mms::ReadRequest& request, const LocalAssociation& local);
//true if every variable in the request is an association object
bool IsAssociationInfoRequest(const mms::ReadRequest& request);

// ---------------------------------------------------------------------------------------------
// Data flags and point values (Block 1)
// ---------------------------------------------------------------------------------------------

//Data_Flags / the quality part of an indication point: BIT STRING (8). Bit numbers count from the most
//significant bit; the byte value is given as a mask.                                               [V]
//   bits 0-1  State (State types only, else 0)   mask 0xC0   0 between, 1 off, 2 on, 3 invalid
//   bits 2-3  Validity                          mask 0x30   0 valid, 1 held, 2 suspect, 3 not valid
//   bits 4-5  CurrentSource                     mask 0x0C   0 telemetered, 1 calculated, 2 entered, 3 estimated
//   bit  6    NormalValue                       mask 0x02   0 normal, 1 abnormal
//   bit  7    TimeStampQuality                  mask 0x01   0 valid, 1 invalid
//The value distribution seen on the wire (state in the top two bits, validity/source/abnormal in the rest, never
//both state and flags in conflict) confirms this layout; the names are from the specification.
enum class StateValue : uint8_t
{
	Between = 0,
	Off = 1,
	On = 2,
	Invalid = 3
};
enum class Validity : uint8_t
{
	Valid = 0,
	Held = 1,
	Suspect = 2,
	NotValid = 3
};
enum class CurrentSource : uint8_t
{
	Telemetered = 0,
	Calculated = 1,
	Entered = 2,
	Estimated = 3
};
struct DataFlags
{
	StateValue state = StateValue::Between;
	Validity validity = Validity::Valid;
	CurrentSource source = CurrentSource::Telemetered;
	bool abnormal = false;
	bool timeStampInvalid = false;
	bool operator==(const DataFlags&) const = default;
};
uint8_t PackFlags(const DataFlags& f);
DataFlags UnpackFlags(uint8_t byte);

//The point value structures. Each is an MMS Data value of the layout noted; [V] seen, [S] specification.
enum class PointType : uint8_t
{
	Real,             //float                                                       [S]
	RealQ,            //STRUCTURE { float, Flags }                                  [V]
	RealQTimeTag,     //STRUCTURE { TimeStamp, float, Flags }                       [S]
	RealExtended,     //STRUCTURE { TimeStampExtended, float, Flags, COV }          [S]
	State,            //BIT STRING (2): state only                                  [S]
	StateQ,           //BIT STRING (8): state and flags in one octet                [V]
	StateQTimeTag,    //STRUCTURE { TimeStamp, StateQ }                             [S]
	StateExtended,    //STRUCTURE { TimeStampExtended, StateQ, COV }                [S]
	Discrete,         //integer                                                     [S]
	DiscreteQ,        //STRUCTURE { integer, Flags }                                [S]
	DiscreteQTimeTag, //STRUCTURE { TimeStamp, integer, Flags }                     [S]
	DiscreteExtended  //STRUCTURE { TimeStampExtended, integer, Flags, COV }        [S]
};
//TimeStamp: integer, seconds since 1970-01-01 UTC. TimeStampExtended: STRUCTURE { TimeStamp, milliseconds: integer }.
//COV: integer, counter of value changes.

struct PointValue
{
	PointType type = PointType::RealQ;
	double real = 0;                        //Real types
	int64_t discrete = 0;                   //Discrete types
	uint8_t flags = 0;                      //the flags octet as on the wire (for State/StateQ types it includes the state bits)
	StateValue state = StateValue::Between; //State types (kept in step with flags by Parse/Make)
	std::optional<int64_t> timeStamp;       //*TimeTag and *Extended
	std::optional<int64_t> milliseconds;    //*Extended
	std::optional<int64_t> cov;             //*Extended
	bool operator==(const PointValue&) const = default;

	DataFlags Flags() const;
};
bool IsRealType(PointType t);
bool IsStateType(PointType t);
bool IsDiscreteType(PointType t);

Data ToData(const PointValue& p);
//Parse as the given type. Throws DecodeError if the data does not have that layout.
PointValue ParsePoint(const Data& data, PointType type);
//Infer the type from the shape. {integer, flags} is ambiguous between DiscreteQ and StateQTimeTag and gives
//DiscreteQ: configure the type where it is known.
std::optional<PointType> InferPointType(const Data& data);
//the MMS type description a server would return for a point type (Get Data Value Type)            [S]
mms::TypeDescription TypeDescriptionOf(PointType type);
//the type description of Bilateral_Table_ID: a visible string of at most 32 characters (the size is written negative: a varying length)  [S]
mms::TypeDescription BilateralTableIdTypeDescription();
std::optional<PointType> ClassifyTypeDescription(const mms::TypeDescription& type);

// ---------------------------------------------------------------------------------------------
// Block 1 services
// ---------------------------------------------------------------------------------------------

//Get Data Value: MMS Read of one or more variables
mms::ReadRequest BuildGetDataValue(const std::vector<ObjectName>& names);
//Get Data Value Names: MMS GetNameList, class named variable; domain given = ICC scope, else VCC scope
mms::GetNameListRequest BuildGetDataValueNames(const std::optional<std::string>& iccDomain,
	const std::optional<std::string>& continueAfter = std::nullopt);
//Get Data Value Type: MMS GetVariableAccessAttributes
mms::GetVariableAccessAttributesRequest BuildGetDataValueType(const ObjectName& name);

//Paging of GetNameList: the request for the next page, or nullopt if the response was the last
//(moreFollows false, which is TRUE when absent, or an empty page).
std::optional<mms::GetNameListRequest> NextNamePage(const mms::GetNameListRequest& request, const mms::GetNameListResponse& response);
//Server side: one page of `all` (in order), after request.continueAfter, at most maxPerPage names. moreFollows is
//omitted for TRUE and written as FALSE on the last page, as real peers do.   nullopt if continueAfter is not in
//`all` (answer with an error). maxPerPage 0 means unlimited.
std::optional<mms::GetNameListResponse> BuildNamePage(const std::vector<std::string>& all, const mms::GetNameListRequest& request,
	size_t maxPerPage);

//Data sets
struct DataSetDefinition
{
	ObjectName name;
	std::vector<ObjectName> members;
	bool operator==(const DataSetDefinition&) const = default;
};
mms::DefineNamedVariableListRequest BuildCreateDataSet(const DataSetDefinition& def);
mms::DeleteNamedVariableListRequest BuildDeleteDataSet(const ObjectName& name);
//Get Data Set Names: GetNameList, class named variable list
mms::GetNameListRequest BuildGetDataSetNames(const std::optional<std::string>& iccDomain,
	const std::optional<std::string>& continueAfter = std::nullopt);
//Get Data Set Element Names: GetNamedVariableListAttributes; the response gives the members
mms::GetNamedVariableListAttributesRequest BuildGetDataSetElementNames(const ObjectName& name);
DataSetDefinition ParseDataSetElementNames(const ObjectName& name, const mms::GetNamedVariableListAttributesResponse& response);
mms::GetNamedVariableListAttributesResponse BuildDataSetElementNamesResponse(const DataSetDefinition& def, bool deletable = true);
//Get Data Set Element Values: Read of the list name
mms::ReadRequest BuildGetDataSetElementValues(const ObjectName& name);
//the results of such a read, checked to be one per member
std::vector<mms::AccessResult> ParseDataSetValues(const DataSetDefinition& def, const mms::ReadResponse& response);
mms::DeleteNamedVariableListResponse BuildDeleteResponse(uint32_t matched, uint32_t deleted);

// ---------------------------------------------------------------------------------------------
// Block 2: data set transfer sets
// ---------------------------------------------------------------------------------------------

//DSConditions (requested and detected): BIT STRING (5), bit i set for condition i.
//Bit numbers confirmed by values on the wire ([V]: integrity timeout in a report); names from the specification.
namespace DsCondition
{
constexpr uint8_t IntervalTimeOut = 1 << 0;
constexpr uint8_t IntegrityTimeOut = 1 << 1;
constexpr uint8_t ObjectChange = 1 << 2;
constexpr uint8_t OperatorRequest = 1 << 3;
constexpr uint8_t OtherExternalEvent = 1 << 4;
}
constexpr size_t DsConditionBitCount = 5;
//bit index i <-> mask 1 << i. The bit count is kept so that a received string rebuilds identically.
struct DsConditions
{
	uint8_t mask = 0;
	size_t bitCount = DsConditionBitCount;
	bool operator==(const DsConditions&) const = default;
	bool Has(uint8_t condition) const
	{
		return (mask & condition) != 0;
	}
};
Data ToData(const DsConditions& c);
DsConditions ParseDsConditions(const Data& data);

//DSTransferSet, the value written to a transfer set to start (or stop) it.
//  [V] 1996-08 peers: 12 members:  DataSetName (object ref), StartTime, Interval, TLE, BufferTime, IntegrityCheck (integers),
//      DSConditionsRequested (bit string), BlockData, Critical, RBE, Status (booleans), EventCodeRequested (integer)
//  [V] 2000-08 peers: 13 members, with AllChangesReported (boolean) between RBE and Status:
//      a real 2000-08 peer writes this form, and refused the 12 member form (data access error type-inconsistent).
//The fourth boolean of the 12 member form is Status (TRUE = start): it is TRUE in every 1996-08 capture, and AllChangesReported,
//which must be FALSE for a 1996-08 peer, is not part of that form (it was added in 2000-08). Status FALSE stops the transfer set. [S] - not seen from real peers.
//Parse accepts both; Build writes 13 members when `allChangesMember` is set (the client sets it for a 2000-08 peer).
//A real 2000-08 peer refused AllChangesReported TRUE with BufferTime 0 (object-attribute-inconsistent) and writes AllChangesReported FALSE itself.
//Its rule: AllChangesReported TRUE needs 2000-08, RBE TRUE, the ObjectChange condition and a BufferTime above 0; otherwise it must be FALSE.
//Times are seconds (StartTime: 0 or seconds since 1970; Interval, TLE, BufferTime, IntegrityCheck: seconds).
//BlockData TRUE asks for the blocked form of the reports (TASE.2 Block 3, see Tase2Blocked.h): a server honours it when it advertises Block 3.
struct DsTransferSet
{
	ObjectRef dataSetName;
	int64_t startTime = 0;
	int64_t interval = 0;
	int64_t tle = 0;
	int64_t bufferTime = 0;
	int64_t integrityCheck = 0;
	DsConditions conditionsRequested;
	bool blockData = false;
	bool critical = false;
	bool rbe = false;
	bool allChangesMember = false;   //the 13 member form, which has the AllChangesReported member (2000-08)
	bool allChangesReported = false; //only in the 13 member form
	bool status = true;              //TRUE = start (enable) the transfer set, FALSE = stop it
	int64_t eventCodeRequested = 0;
	bool operator==(const DsTransferSet&) const = default;
};
Data ToData(const DsTransferSet& t);
DsTransferSet ParseDsTransferSet(const Data& data);
//Start Transfer: MMS Write of the structure to the transfer set object (an ICC object, named by the server's
//Next_DSTransfer_Set).  [V]
mms::WriteRequest BuildStartTransfer(const ObjectName& transferSet, const DsTransferSet& parameters);
//Stop Transfer: the same write with Status FALSE. [S] - not seen from real peers.
mms::WriteRequest BuildStopTransfer(const ObjectName& transferSet, DsTransferSet parameters);
//Server side: parse a Write as a transfer set write (single variable, single structure)
struct TransferSetWrite
{
	ObjectName transferSet;
	DsTransferSet parameters;
	bool operator==(const TransferSetWrite&) const = default;
};
TransferSetWrite ParseTransferSetWrite(const mms::WriteRequest& request);
mms::WriteResponse BuildWriteSuccess(size_t count = 1);

//Next_DSTransfer_Set: read in the ICC domain; the result is the name of the next free transfer set       [V]
mms::ReadRequest BuildGetNextDsTransferSet(const std::string& iccDomain);
//Throws DecodeError if the read failed or the result is not an object reference
ObjectRef ParseNextDsTransferSet(const mms::ReadResponse& response);
//Server side. No free transfer set: the data access error object-non-existent (not seen on the wire). [S]
mms::ReadResponse BuildNextDsTransferSetResponse(const std::optional<ObjectRef>& next);

//The special objects at the head of a report's data set, and their data
Data TransferSetTimeStampData(int64_t seconds); //integer                          [S]
Data EventCodeData(int64_t code);               //integer                          [V]
int64_t ParseTransferSetTimeStamp(const Data& d);
int64_t ParseEventCode(const Data& d);

enum class Special : uint8_t
{
	TransferSetName,
	TimeStamp,
	ConditionsDetected,
	EventCode
};

//An InformationReport of a transfer set. The report's variable access specification is either the data set's
//name (variableListName) or, DEVIATION seen from real peers [V], a listOfVariable naming every member inline; the
//results are the special objects first (in the order of the data set definition), then the member values.
struct TransferReport
{
	mms::VariableAccessSpecification specification; //the data set name, or the members listed inline
	//the special objects at the head, in order; always starts with TransferSetName
	std::vector<Special> layout{Special::TransferSetName};
	ObjectRef transferSet;
	std::optional<int64_t> timeStamp;
	std::optional<DsConditions> conditionsDetected;
	std::optional<int64_t> eventCode;
	std::vector<mms::AccessResult> values;
	bool operator==(const TransferReport&) const = default;

	//the data set name, if the report names one (nullptr for an inline member list)
	const ObjectName* DataSetName() const
	{
		return std::get_if<ObjectName>(&specification);
	}
};
//members: the data set definition (all members, specials included), if known. With it the leading members named
//Transfer_Set_Name, Transfer_Set_Time_Stamp, DSConditions_Detected or Event_Code_Detected (by item name, either scope)
//are decoded as the special objects; the number of members must equal the number of results. A report that lists its
//variables inline carries its own definition and `members` is not used.
//Without a definition the first result must be the Transfer_Set_Name and everything after it is a value.
//Throws DecodeError otherwise.
TransferReport ParseTransferReport(const mms::InformationReport& report, const std::vector<ObjectName>* members = nullptr);
mms::InformationReport BuildTransferReport(const TransferReport& report);

//The acknowledgement of a transfer report by the client (the standard's Transfer_Report_ACK / Transfer_Report_NACK). It is an unconfirmed InformationReport
//(sent in the direction opposite to the report) whose variable access specification is a list of variables, each named in the domain of the transfer set:
//  ACK:  Transfer_Report_ACK  (the transfer set's object name, as in the report received),
//        Transfer_Set_Time_Stamp (the time stamp of the report received; left out when the report carried none)
//  NACK: Transfer_Report_NACK (the transfer set's object name, as in the report received)
//The standard names the variables and their meaning [T]; the order of the variables, and leaving the time stamp out, are this port's reading [S].
//The names are those of the report received, so a Vcc (vmd-specific) transfer set is acknowledged with vmd-specific variables.
mms::InformationReport BuildTransferReportAck(const ObjectRef& transferSet, const std::optional<int64_t>& timeStamp);
mms::InformationReport BuildTransferReportNack(const ObjectRef& transferSet);
struct ReportAck
{
	bool negative = false; //Transfer_Report_NACK
	ObjectRef transferSet;
	std::optional<int64_t> timeStamp; //Transfer_Set_Time_Stamp, if the report carried one
	bool operator==(const ReportAck&) const = default;
};
//Recognise an acknowledgement. Lenient on receipt: the variables may come in any order, other variables are ignored, the time stamp may be absent
//(a NACK carries none) and a time stamp that cannot be read is treated as absent. Returns nullopt if the report names neither
//Transfer_Report_ACK nor Transfer_Report_NACK (by item name, whatever the scope or domain), so that it is some other report. Throws DecodeError if it
//does name one but the object name value is missing or cannot be read.
std::optional<ReportAck> ParseReportAck(const mms::InformationReport& report);

} // namespace tase2
} // namespace iccp

#endif /* ICCP_TASE2_H_ */
