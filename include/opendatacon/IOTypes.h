/*	opendatacon
 *
 *	Copyright (c) 2015:
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
 * IOTypes.h
 *
 *  Re-Created on: 2018-06-24
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef IOTYPES_H_
#define IOTYPES_H_

#include <opendatacon/EnumClassFlags.h>
#include <opendatacon/util.h>
#include <opendatacon/asio.h>
#include <string>
#include <tuple>

namespace odc
{

//enumerate all the different type of events that can pass through opendatacon
//As a starting point, define values to correspond with the previously used dnp3 measurment and output types, and some extras
// that way it should be easy to migrate from using actual opendnp3 library types
enum class EventType: uint8_t
{
	//Used to start iteration
	//Must be first value
	BeforeRange               = 0,

	//Inputs
	Binary                    = 1,
	DoubleBitBinary           = 2,
	Analog                    = 3,
	Counter                   = 4,
	FrozenCounter             = 5,
	BinaryOutputStatus        = 6,
	AnalogOutputStatus        = 7,
	BinaryCommandEvent        = 8,
	AnalogCommandEvent        = 9,
	OctetString               = 10,
	TimeAndInterval           = 11,
	SecurityStat              = 12,

	Reserved1                 = 13,
	Reserved2                 = 14,
	Reserved3                 = 15,

	//Outputs
	ControlRelayOutputBlock   = 16,
	AnalogOutputInt16         = 17,
	AnalogOutputInt32         = 18,
	AnalogOutputFloat32       = 19,
	AnalogOutputDouble64      = 20,

	Reserved4                 = 21,
	Reserved5                 = 22,
	Reserved6                 = 23,

	//Quality (for when the quality changes, but not the value)
	BinaryQuality             = 24,
	DoubleBitBinaryQuality    = 25,
	AnalogQuality             = 26,
	CounterQuality            = 27,
	BinaryOutputStatusQuality = 28,
	FrozenCounterQuality      = 29,
	AnalogOutputStatusQuality = 30,
	OctetStringQuality        = 31,

	Reserved8                 = 32,
	Reserved9                 = 33,

	//File Control
	FileAuth                  = 34,
	FileCommand               = 35,
	FileCommandStatus         = 36,
	FileTransport             = 37,
	FileTransportStatus       = 38,
	FileDescriptor            = 39,
	FileSpecString            = 40,

	Reserved10                = 41,
	Reserved11                = 42,
	Reserved12                = 43, //TODO: Add a 'system clock offset' event type so that ports with external time sync can pass through
	                                //	could also be sent externally to indicate if the system clock has drifted
	//Connection events
	ConnectState              = 44,

	//Used to end iteration
	//Must be last value
	AfterRange                = 45
};
constexpr EventType operator +( const EventType lhs, const int rhs)
{
	return static_cast<EventType>(static_cast<const uint8_t>(lhs) + rhs);
}

enum class CommandStatus : uint8_t
{
	SUCCESS = 0,
	TIMEOUT = 1,
	NO_SELECT = 2,
	FORMAT_ERROR = 3,
	NOT_SUPPORTED = 4,
	ALREADY_ACTIVE = 5,
	HARDWARE_ERROR = 6,
	LOCAL = 7,
	TOO_MANY_OPS = 8,
	NOT_AUTHORIZED = 9,
	AUTOMATION_INHIBIT = 10,
	PROCESSING_LIMITED = 11,
	OUT_OF_RANGE = 12,
	DOWNSTREAM_LOCAL = 13,
	ALREADY_COMPLETE = 14,
	BLOCKED = 15,
	CANCELLED = 16,
	BLOCKED_OTHER_MASTER = 17,
	DOWNSTREAM_FAIL = 18,
	NON_PARTICIPATING = 126,
	UNDEFINED = 127
};

enum class ControlCode : uint8_t
{
	NUL = 1,
	PULSE_ON = 3,
	PULSE_OFF = 5,
	LATCH_ON = 7,
	LATCH_OFF = 9,
	CLOSE_PULSE_ON = 11,
	TRIP_PULSE_ON = 13,
	UNDEFINED = 15
};

//TODO: make these ToString functions faster
//	use hash map cache
#define ENUMSTRING(A,E,B) if(A == E::B) return #B;

inline std::string ToString(const CommandStatus cs)
{
	ENUMSTRING(cs, CommandStatus,SUCCESS )
	ENUMSTRING(cs, CommandStatus,TIMEOUT)
	ENUMSTRING(cs, CommandStatus,NO_SELECT)
	ENUMSTRING(cs, CommandStatus,FORMAT_ERROR)
	ENUMSTRING(cs, CommandStatus,NOT_SUPPORTED)
	ENUMSTRING(cs, CommandStatus,ALREADY_ACTIVE)
	ENUMSTRING(cs, CommandStatus,HARDWARE_ERROR)
	ENUMSTRING(cs, CommandStatus,LOCAL)
	ENUMSTRING(cs, CommandStatus,TOO_MANY_OPS)
	ENUMSTRING(cs, CommandStatus,NOT_AUTHORIZED)
	ENUMSTRING(cs, CommandStatus,AUTOMATION_INHIBIT)
	ENUMSTRING(cs, CommandStatus,PROCESSING_LIMITED)
	ENUMSTRING(cs, CommandStatus,OUT_OF_RANGE)
	ENUMSTRING(cs, CommandStatus,DOWNSTREAM_LOCAL)
	ENUMSTRING(cs, CommandStatus,ALREADY_COMPLETE)
	ENUMSTRING(cs, CommandStatus,BLOCKED)
	ENUMSTRING(cs, CommandStatus,CANCELLED)
	ENUMSTRING(cs, CommandStatus,BLOCKED_OTHER_MASTER)
	ENUMSTRING(cs, CommandStatus,DOWNSTREAM_FAIL)
	ENUMSTRING(cs, CommandStatus,NON_PARTICIPATING)
	ENUMSTRING(cs, CommandStatus,UNDEFINED)
	return "<no_string_representation>";
}

inline std::string ToString(const ControlCode cc)
{
	ENUMSTRING(cc,ControlCode,NUL                  )
	ENUMSTRING(cc,ControlCode,PULSE_ON             )
	ENUMSTRING(cc,ControlCode,PULSE_OFF            )
	ENUMSTRING(cc,ControlCode,LATCH_ON             )
	ENUMSTRING(cc,ControlCode,LATCH_OFF            )
	ENUMSTRING(cc,ControlCode,CLOSE_PULSE_ON       )
	ENUMSTRING(cc,ControlCode,TRIP_PULSE_ON        )
	ENUMSTRING(cc,ControlCode,UNDEFINED            )
	return "<no_string_representation>";
}

inline std::string ToString(const EventType et)
{
	ENUMSTRING(et,EventType,Binary                   )
	ENUMSTRING(et,EventType,DoubleBitBinary          )
	ENUMSTRING(et,EventType,Analog                   )
	ENUMSTRING(et,EventType,Counter                  )
	ENUMSTRING(et,EventType,FrozenCounter            )
	ENUMSTRING(et,EventType,BinaryOutputStatus       )
	ENUMSTRING(et,EventType,AnalogOutputStatus       )
	ENUMSTRING(et,EventType,BinaryCommandEvent       )
	ENUMSTRING(et,EventType,AnalogCommandEvent       )
	ENUMSTRING(et,EventType,OctetString              )
	ENUMSTRING(et,EventType,TimeAndInterval          )
	ENUMSTRING(et,EventType,SecurityStat             )
	ENUMSTRING(et,EventType,ControlRelayOutputBlock  )
	ENUMSTRING(et,EventType,AnalogOutputInt16        )
	ENUMSTRING(et,EventType,AnalogOutputInt32        )
	ENUMSTRING(et,EventType,AnalogOutputFloat32      )
	ENUMSTRING(et,EventType,AnalogOutputDouble64     )
	ENUMSTRING(et,EventType,BinaryQuality            )
	ENUMSTRING(et,EventType,DoubleBitBinaryQuality   )
	ENUMSTRING(et,EventType,AnalogQuality            )
	ENUMSTRING(et,EventType,CounterQuality           )
	ENUMSTRING(et,EventType,BinaryOutputStatusQuality)
	ENUMSTRING(et,EventType,FrozenCounterQuality     )
	ENUMSTRING(et,EventType,AnalogOutputStatusQuality)
	ENUMSTRING(et,EventType,OctetStringQuality       )
	ENUMSTRING(et,EventType,FileAuth                 )
	ENUMSTRING(et,EventType,FileCommand              )
	ENUMSTRING(et,EventType,FileCommandStatus        )
	ENUMSTRING(et,EventType,FileTransport            )
	ENUMSTRING(et,EventType,FileTransportStatus      )
	ENUMSTRING(et,EventType,FileDescriptor           )
	ENUMSTRING(et,EventType,FileSpecString           )
	ENUMSTRING(et,EventType,ConnectState             )
	ENUMSTRING(et,EventType,Reserved1                )
	ENUMSTRING(et,EventType,Reserved2                )
	ENUMSTRING(et,EventType,Reserved3                )
	ENUMSTRING(et,EventType,Reserved4                )
	ENUMSTRING(et,EventType,Reserved5                )
	ENUMSTRING(et,EventType,Reserved6                )
	ENUMSTRING(et,EventType,Reserved8                )
	ENUMSTRING(et,EventType,Reserved9                )
	ENUMSTRING(et,EventType,Reserved10               )
	ENUMSTRING(et,EventType,Reserved11               )
	ENUMSTRING(et,EventType,Reserved12               )
	return "<no_string_representation>";
}

//Quatilty flags that can be used for any EventType
//Start with a superset of all the dnp3 type qualities
enum class QualityFlags: uint16_t
{
	NONE              = 0,
	ONLINE            = 1<<0,
	RESTART           = 1<<1,
	COMM_LOST         = 1<<2,
	REMOTE_FORCED     = 1<<3,
	LOCAL_FORCED      = 1<<4,
	OVERRANGE         = 1<<5,
	REFERENCE_ERR     = 1<<6,
	ROLLOVER          = 1<<7,
	DISCONTINUITY     = 1<<8,
	CHATTER_FILTER    = 1<<9
};
ENABLE_BITWISE(QualityFlags)

#define FLAGSTRING(E,X) if((q & E::X) == E::X) s += #X "|";
//TODO: this could be a template so it works for any BitwiseEnabled<> class
inline std::string ToString(const QualityFlags q)
{
	std::string s = "|";
	FLAGSTRING(QualityFlags,ONLINE        )
	FLAGSTRING(QualityFlags,RESTART       )
	FLAGSTRING(QualityFlags,COMM_LOST     )
	FLAGSTRING(QualityFlags,REMOTE_FORCED )
	FLAGSTRING(QualityFlags,LOCAL_FORCED  )
	FLAGSTRING(QualityFlags,OVERRANGE     )
	FLAGSTRING(QualityFlags,REFERENCE_ERR )
	FLAGSTRING(QualityFlags,ROLLOVER      )
	FLAGSTRING(QualityFlags,DISCONTINUITY )
	FLAGSTRING(QualityFlags,CHATTER_FILTER)
	if(s == "|") return "|NONE|";
	return s;
}
#define SINGLEFLAGSTRING(E,X) if(q == E::X) return #X;
//TODO: this could be a template so it works for any BitwiseEnabled<> class
inline std::string SingleFlagString(const QualityFlags q)
{
	SINGLEFLAGSTRING(QualityFlags,NONE          )
	SINGLEFLAGSTRING(QualityFlags,ONLINE        )
	SINGLEFLAGSTRING(QualityFlags,RESTART       )
	SINGLEFLAGSTRING(QualityFlags,COMM_LOST     )
	SINGLEFLAGSTRING(QualityFlags,REMOTE_FORCED )
	SINGLEFLAGSTRING(QualityFlags,LOCAL_FORCED  )
	SINGLEFLAGSTRING(QualityFlags,OVERRANGE     )
	SINGLEFLAGSTRING(QualityFlags,REFERENCE_ERR )
	SINGLEFLAGSTRING(QualityFlags,ROLLOVER      )
	SINGLEFLAGSTRING(QualityFlags,DISCONTINUITY )
	SINGLEFLAGSTRING(QualityFlags,CHATTER_FILTER)
	return "MULTIPLE_FLAGS";
}

struct ControlRelayOutputBlock
{
	ControlCode functionCode = ControlCode::LATCH_ON;
	uint8_t count = 1;
	uint32_t onTimeMS = 100;
	uint32_t offTimeMS = 100;
	CommandStatus status = CommandStatus::SUCCESS;

	explicit operator std::string() const
	{
		return "|"+ToString(functionCode)+"|Count "+std::to_string(count)+
		       "|ON "+std::to_string(onTimeMS)+"ms|OFF "+std::to_string(offTimeMS)+"ms|";
	}
};
inline bool operator==(const ControlRelayOutputBlock& lhs, const ControlRelayOutputBlock& rhs)
{
	return lhs.functionCode == rhs.functionCode &&
	       lhs.count == rhs.count &&
	       lhs.onTimeMS == rhs.onTimeMS &&
	       lhs.offTimeMS == rhs.offTimeMS &&
	       lhs.status == rhs.status;
}
inline bool operator!=(const ControlRelayOutputBlock& lhs, const ControlRelayOutputBlock& rhs)
{
	return !(lhs == rhs);
}
inline std::string ToString(const ControlRelayOutputBlock& crob)
{
	return std::string(crob);
}

enum class ConnectState {PORT_UP,CONNECTED,DISCONNECTED,PORT_DOWN,UNDEFINED};
inline std::string ToString(const ConnectState cs)
{
	ENUMSTRING(cs, ConnectState, PORT_UP)
	ENUMSTRING(cs, ConnectState, CONNECTED)
	ENUMSTRING(cs, ConnectState, DISCONNECTED)
	ENUMSTRING(cs, ConnectState, PORT_DOWN)
	return "<no_string_representation>";
}

inline QualityFlags QualityFlagsFromString(const std::string& StrQuality)
{
	#define CHECKFLAGSTRING(X) if (StrQuality.find(#X) != std::string::npos) QualityResult |= QualityFlags::X

	QualityFlags QualityResult = QualityFlags::NONE;

	CHECKFLAGSTRING(ONLINE);
	CHECKFLAGSTRING(RESTART);
	CHECKFLAGSTRING(COMM_LOST);
	CHECKFLAGSTRING(REMOTE_FORCED);
	CHECKFLAGSTRING(LOCAL_FORCED);
	CHECKFLAGSTRING(OVERRANGE);
	CHECKFLAGSTRING(REFERENCE_ERR);
	CHECKFLAGSTRING(ROLLOVER);
	CHECKFLAGSTRING(DISCONTINUITY);
	CHECKFLAGSTRING(CHATTER_FILTER);

	return QualityResult;
}

#define CHECKENUMSTRING(S,C,X) if (S == #X) return C::X

inline CommandStatus CommandStatusFromString(const std::string& StrCommandStatus)
{
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,SUCCESS);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,TIMEOUT);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,NO_SELECT);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,FORMAT_ERROR);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,NOT_SUPPORTED);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,ALREADY_ACTIVE);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,HARDWARE_ERROR);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,LOCAL);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,TOO_MANY_OPS);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,NOT_AUTHORIZED);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,AUTOMATION_INHIBIT);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,PROCESSING_LIMITED);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,OUT_OF_RANGE);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,DOWNSTREAM_LOCAL);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,ALREADY_COMPLETE);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,BLOCKED);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,CANCELLED);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,BLOCKED_OTHER_MASTER);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,DOWNSTREAM_FAIL);
	CHECKENUMSTRING(StrCommandStatus,CommandStatus,NON_PARTICIPATING);
	return CommandStatus::UNDEFINED;
}

inline EventType EventTypeFromString(const std::string& StrEventType)
{
	auto EventTypeResult = EventType::BeforeRange;
	do
	{
		EventTypeResult = EventTypeResult+1;
		if (StrEventType == ToString(EventTypeResult))
			break;
	} while (EventTypeResult != EventType::AfterRange);
	return EventTypeResult;
}

inline ControlCode ControlCodeFromString(const std::string& StrControlCode)
{
	CHECKENUMSTRING(StrControlCode,ControlCode,CLOSE_PULSE_ON);
	CHECKENUMSTRING(StrControlCode,ControlCode,LATCH_OFF);
	CHECKENUMSTRING(StrControlCode,ControlCode,LATCH_ON);
	CHECKENUMSTRING(StrControlCode,ControlCode,NUL);
	CHECKENUMSTRING(StrControlCode,ControlCode,PULSE_OFF);
	CHECKENUMSTRING(StrControlCode,ControlCode,PULSE_ON);
	CHECKENUMSTRING(StrControlCode,ControlCode,TRIP_PULSE_ON);
	return ControlCode::UNDEFINED;
}

inline ConnectState ConnectStateFromString(const std::string StrConnectState)
{
	CHECKENUMSTRING(StrConnectState,ConnectState,CONNECTED);
	CHECKENUMSTRING(StrConnectState,ConnectState,DISCONNECTED);
	CHECKENUMSTRING(StrConnectState,ConnectState,PORT_DOWN);
	CHECKENUMSTRING(StrConnectState,ConnectState,PORT_UP);
	return ConnectState::UNDEFINED;
}

#define CHECKENUMINT(I,C,X) if (I == static_cast<int>(C::X)) return C::X

inline CommandStatus CommandStatusFromInt(const int i)
{
	CHECKENUMINT(i,CommandStatus,SUCCESS);
	CHECKENUMINT(i,CommandStatus,TIMEOUT);
	CHECKENUMINT(i,CommandStatus,NO_SELECT);
	CHECKENUMINT(i,CommandStatus,FORMAT_ERROR);
	CHECKENUMINT(i,CommandStatus,NOT_SUPPORTED);
	CHECKENUMINT(i,CommandStatus,ALREADY_ACTIVE);
	CHECKENUMINT(i,CommandStatus,HARDWARE_ERROR);
	CHECKENUMINT(i,CommandStatus,LOCAL);
	CHECKENUMINT(i,CommandStatus,TOO_MANY_OPS);
	CHECKENUMINT(i,CommandStatus,NOT_AUTHORIZED);
	CHECKENUMINT(i,CommandStatus,AUTOMATION_INHIBIT);
	CHECKENUMINT(i,CommandStatus,PROCESSING_LIMITED);
	CHECKENUMINT(i,CommandStatus,OUT_OF_RANGE);
	CHECKENUMINT(i,CommandStatus,DOWNSTREAM_LOCAL);
	CHECKENUMINT(i,CommandStatus,ALREADY_COMPLETE);
	CHECKENUMINT(i,CommandStatus,BLOCKED);
	CHECKENUMINT(i,CommandStatus,CANCELLED);
	CHECKENUMINT(i,CommandStatus,BLOCKED_OTHER_MASTER);
	CHECKENUMINT(i,CommandStatus,DOWNSTREAM_FAIL);
	CHECKENUMINT(i,CommandStatus,NON_PARTICIPATING);
	return CommandStatus::UNDEFINED;
}

inline EventType EventTypeFromInt(const int i)
{
	if(i <= static_cast<int>(EventType::BeforeRange) || i >= static_cast<int>(EventType::AfterRange))
		return EventType::AfterRange;
	return static_cast<EventType>(i);
}

inline ControlCode ControlCodeFromInt(const int i)
{
	CHECKENUMINT(i,ControlCode,NUL);
	CHECKENUMINT(i,ControlCode,PULSE_ON);
	CHECKENUMINT(i,ControlCode,PULSE_OFF);
	CHECKENUMINT(i,ControlCode,LATCH_ON);
	CHECKENUMINT(i,ControlCode,LATCH_OFF);
	CHECKENUMINT(i,ControlCode,CLOSE_PULSE_ON);
	CHECKENUMINT(i,ControlCode,TRIP_PULSE_ON);
	return ControlCode::UNDEFINED;
}

//Map EventTypes to payload types
template<EventType t> struct EventTypePayload { typedef void type; };
#define EVENTPAYLOAD(E,T)\
	template<> struct EventTypePayload<E> { typedef T type; };

//TODO: make these structs?
typedef std::pair<bool,bool> DBB;
typedef std::tuple<msSinceEpoch_t,uint32_t,uint8_t> TAI;
typedef std::pair<uint16_t,uint32_t> SS;
typedef std::pair<int16_t,CommandStatus> AO16;
typedef std::pair<int32_t,CommandStatus> AO32;
typedef std::pair<float,CommandStatus> AOF;
typedef std::pair<double,CommandStatus> AOD;

//payload for odc octet strings derrive from asio::const_buffer,
//	so it can be written directly to a socket, or added to a collection to be written to socket etc.
struct OctetStringBuffer: public shared_const_buffer
{
	//by default, has_appropriate_members is a false type
	template<typename T, typename = void>
	struct has_appropriate_members: std::false_type {};

	//these will all resolve for types with the needed members
	template<typename T>
	using data_member_t = decltype(std::declval<T>().data());
	template<typename T>
	using size_member_t = decltype(std::declval<T>().size());
	template<typename T>
	using get_allocator_member_t = decltype(std::declval<T>().get_allocator());

	//if T has data(), size() and get_allocator() members
	//then has_appropriate_members is a true type
	template<typename T>
	struct has_appropriate_members<T, std::void_t<data_member_t<T>, size_member_t<T>, get_allocator_member_t<T>>>
		: std::true_type {};

	template<typename T, typename = std::enable_if_t<has_appropriate_members<T>::value>>
	OctetStringBuffer(T&& aContainer):
		//shared_const_buffer is a ref counted wraper that will delete the data in good time
		//odc::make_shared ensures construct/destroy both done in libODC
		shared_const_buffer(odc::make_shared(std::move(aContainer)))
	{}

	OctetStringBuffer(): //by default, make a 'null string'
		shared_const_buffer(odc::make_shared(std::string("")))
	{}
};
inline bool operator==(const OctetStringBuffer& lhs, const OctetStringBuffer& rhs)
{
	if(lhs.size() != rhs.size())
		return false;
	for (size_t i = 0; i < lhs.size(); i++)
	{
		if (*((uint8_t*)lhs.data()+i) != *((uint8_t*)rhs.data()+i))
			return false;
	}
	return true;
}
inline bool operator!=(const OctetStringBuffer& lhs, const OctetStringBuffer& rhs)
{
	return !(lhs == rhs);
}

EVENTPAYLOAD(EventType::Binary                   , bool)
EVENTPAYLOAD(EventType::DoubleBitBinary          , DBB)
EVENTPAYLOAD(EventType::Analog                   , double)
EVENTPAYLOAD(EventType::Counter                  , uint32_t)
EVENTPAYLOAD(EventType::FrozenCounter            , uint32_t)
EVENTPAYLOAD(EventType::BinaryOutputStatus       , bool)
EVENTPAYLOAD(EventType::AnalogOutputStatus       , double)
EVENTPAYLOAD(EventType::BinaryCommandEvent       , CommandStatus)
EVENTPAYLOAD(EventType::AnalogCommandEvent       , CommandStatus)
EVENTPAYLOAD(EventType::OctetString              , OctetStringBuffer)
EVENTPAYLOAD(EventType::TimeAndInterval          , TAI)
EVENTPAYLOAD(EventType::SecurityStat             , SS)
EVENTPAYLOAD(EventType::ControlRelayOutputBlock  , ControlRelayOutputBlock)
EVENTPAYLOAD(EventType::AnalogOutputInt16        , AO16)
EVENTPAYLOAD(EventType::AnalogOutputInt32        , AO32)
EVENTPAYLOAD(EventType::AnalogOutputFloat32      , AOF)
EVENTPAYLOAD(EventType::AnalogOutputDouble64     , AOD)
EVENTPAYLOAD(EventType::BinaryQuality            , QualityFlags)
EVENTPAYLOAD(EventType::DoubleBitBinaryQuality   , QualityFlags)
EVENTPAYLOAD(EventType::AnalogQuality            , QualityFlags)
EVENTPAYLOAD(EventType::CounterQuality           , QualityFlags)
EVENTPAYLOAD(EventType::BinaryOutputStatusQuality, QualityFlags)
EVENTPAYLOAD(EventType::FrozenCounterQuality     , QualityFlags)
EVENTPAYLOAD(EventType::AnalogOutputStatusQuality, QualityFlags)
EVENTPAYLOAD(EventType::OctetStringQuality       , QualityFlags)
EVENTPAYLOAD(EventType::FileAuth                 , char) //stub
EVENTPAYLOAD(EventType::FileCommand              , char) //stub
EVENTPAYLOAD(EventType::FileCommandStatus        , char) //stub
EVENTPAYLOAD(EventType::FileTransport            , char) //stub
EVENTPAYLOAD(EventType::FileTransportStatus      , char) //stub
EVENTPAYLOAD(EventType::FileDescriptor           , char) //stub
EVENTPAYLOAD(EventType::FileSpecString           , char) //stub
EVENTPAYLOAD(EventType::ConnectState             , ConnectState)
EVENTPAYLOAD(EventType::Reserved1                , char) //stub
EVENTPAYLOAD(EventType::Reserved2                , char) //stub
EVENTPAYLOAD(EventType::Reserved3                , char) //stub
EVENTPAYLOAD(EventType::Reserved4                , char) //stub
EVENTPAYLOAD(EventType::Reserved5                , char) //stub
EVENTPAYLOAD(EventType::Reserved6                , char) //stub
EVENTPAYLOAD(EventType::Reserved8                , char) //stub
EVENTPAYLOAD(EventType::Reserved9                , char) //stub
EVENTPAYLOAD(EventType::Reserved10               , char) //stub
EVENTPAYLOAD(EventType::Reserved11               , char) //stub
EVENTPAYLOAD(EventType::Reserved12               , char) //stub
//TODO: map the rest

enum class DataToStringMethod : uint8_t {Raw,Hex,Base64};
//FIXME: format properly using specialisation https://fmt.dev/latest/api.html#udt
inline auto format_as(DataToStringMethod dtsm) { return fmt::underlying(dtsm); }
inline std::string ToString(const OctetStringBuffer& OSB, DataToStringMethod method = DataToStringMethod::Hex)
{
	auto chardata = static_cast<const char*>(OSB.data());
	auto rawdata = static_cast<const uint8_t*>(OSB.data());
	switch(method)
	{
		case DataToStringMethod::Raw:
			return std::string(chardata,OSB.size());
		case DataToStringMethod::Hex:
			return buf2hex(rawdata,OSB.size());
		case DataToStringMethod::Base64:
			return b64encode(rawdata,OSB.size());
		default:
			return fmt::format("Unsupported DataToStringMethod: {}",method);
	}
}

//see IOTypesString.cpp for these implementations:
template <typename T> T PayloadFromString(const std::string& PayloadStr);
template <> bool PayloadFromString(const std::string& PayloadStr);
template<> DBB PayloadFromString(const std::string& PayloadStr);
template<> double PayloadFromString(const std::string& PayloadStr);
template<> uint32_t PayloadFromString(const std::string& PayloadStr);
template<> CommandStatus PayloadFromString(const std::string& PayloadStr);
OctetStringBuffer PayloadFromString(const std::string& PayloadStr, DataToStringMethod D2S);
template<> TAI PayloadFromString(const std::string& PayloadStr);
template<> SS PayloadFromString(const std::string& PayloadStr);
template<> ControlRelayOutputBlock PayloadFromString(const std::string& PayloadStr);
template<> AO16 PayloadFromString(const std::string& PayloadStr);
template<> AO32 PayloadFromString(const std::string& PayloadStr);
template<> AOF PayloadFromString(const std::string& PayloadStr);
template<> AOD PayloadFromString(const std::string& PayloadStr);
template<> QualityFlags PayloadFromString(const std::string& PayloadStr);
template<> char PayloadFromString(const std::string& PayloadStr);
template<> ConnectState PayloadFromString(const std::string& PayloadStr);

#define DELETEPAYLOADCASE(T)\
	case T: \
		delete static_cast<typename EventTypePayload<T>::type*>(pPayload); \
		break;
#define COPYPAYLOADCASE(T)\
	case T: \
		pPayload = static_cast<void*>(new typename EventTypePayload<T>::type(evt.GetPayload<T>())); \
		break;
#define DEFAULTPAYLOADCASE(T)\
	case T: \
		pPayload = static_cast<void*>(new typename EventTypePayload<T>::type()); \
		break;
#define STRINGPAYLOADCASE(T)\
	case T: \
		SetPayload<T>(PayloadFromString<EventTypePayload<T>::type>(PayloadStr)); \
		break;

class EventInfo
{
public:
	EventInfo(EventType tp, size_t ind = 0, const std::string& source = "",
		QualityFlags qual = QualityFlags::ONLINE,
		msSinceEpoch_t time = msSinceEpoch()):
		Index(ind),
		Timestamp(time),
		Quality(qual),
		SourcePort(source),
		Type(tp),
		pPayload(nullptr)
	{}
	//deep copy for payload
	EventInfo(const EventInfo& evt):
		Index(evt.Index),
		Timestamp(evt.Timestamp),
		Quality(evt.Quality),
		SourcePort(evt.SourcePort),
		Type(evt.Type),
		pPayload(evt.pPayload)
	{
		if(pPayload)
		{
			switch(Type)
			{
				COPYPAYLOADCASE(EventType::Binary                   )
				COPYPAYLOADCASE(EventType::DoubleBitBinary          )
				COPYPAYLOADCASE(EventType::Analog                   )
				COPYPAYLOADCASE(EventType::Counter                  )
				COPYPAYLOADCASE(EventType::FrozenCounter            )
				COPYPAYLOADCASE(EventType::BinaryOutputStatus       )
				COPYPAYLOADCASE(EventType::AnalogOutputStatus       )
				COPYPAYLOADCASE(EventType::BinaryCommandEvent       )
				COPYPAYLOADCASE(EventType::AnalogCommandEvent       )
				COPYPAYLOADCASE(EventType::OctetString              )
				COPYPAYLOADCASE(EventType::TimeAndInterval          )
				COPYPAYLOADCASE(EventType::SecurityStat             )
				COPYPAYLOADCASE(EventType::ControlRelayOutputBlock  )
				COPYPAYLOADCASE(EventType::AnalogOutputInt16        )
				COPYPAYLOADCASE(EventType::AnalogOutputInt32        )
				COPYPAYLOADCASE(EventType::AnalogOutputFloat32      )
				COPYPAYLOADCASE(EventType::AnalogOutputDouble64     )
				COPYPAYLOADCASE(EventType::BinaryQuality            )
				COPYPAYLOADCASE(EventType::DoubleBitBinaryQuality   )
				COPYPAYLOADCASE(EventType::AnalogQuality            )
				COPYPAYLOADCASE(EventType::CounterQuality           )
				COPYPAYLOADCASE(EventType::BinaryOutputStatusQuality)
				COPYPAYLOADCASE(EventType::FrozenCounterQuality     )
				COPYPAYLOADCASE(EventType::AnalogOutputStatusQuality)
				COPYPAYLOADCASE(EventType::OctetStringQuality       )
				COPYPAYLOADCASE(EventType::FileAuth                 )
				COPYPAYLOADCASE(EventType::FileCommand              )
				COPYPAYLOADCASE(EventType::FileCommandStatus        )
				COPYPAYLOADCASE(EventType::FileTransport            )
				COPYPAYLOADCASE(EventType::FileTransportStatus      )
				COPYPAYLOADCASE(EventType::FileDescriptor           )
				COPYPAYLOADCASE(EventType::FileSpecString           )
				COPYPAYLOADCASE(EventType::ConnectState             )
				COPYPAYLOADCASE(EventType::Reserved1                )
				COPYPAYLOADCASE(EventType::Reserved2                )
				COPYPAYLOADCASE(EventType::Reserved3                )
				COPYPAYLOADCASE(EventType::Reserved4                )
				COPYPAYLOADCASE(EventType::Reserved5                )
				COPYPAYLOADCASE(EventType::Reserved6                )
				COPYPAYLOADCASE(EventType::Reserved8                )
				COPYPAYLOADCASE(EventType::Reserved9                )
				COPYPAYLOADCASE(EventType::Reserved10               )
				COPYPAYLOADCASE(EventType::Reserved11               )
				COPYPAYLOADCASE(EventType::Reserved12               )
				default:
					std::string msg = "odc::EventInfo copy-ctor can't handle EventType::"+ToString(Type);
					throw std::runtime_error(msg);
					break;
			}
		}
	}

	~EventInfo()
	{
		if(pPayload)
		{
			switch(Type)
			{
				DELETEPAYLOADCASE(EventType::Binary                   )
				DELETEPAYLOADCASE(EventType::DoubleBitBinary          )
				DELETEPAYLOADCASE(EventType::Analog                   )
				DELETEPAYLOADCASE(EventType::Counter                  )
				DELETEPAYLOADCASE(EventType::FrozenCounter            )
				DELETEPAYLOADCASE(EventType::BinaryOutputStatus       )
				DELETEPAYLOADCASE(EventType::AnalogOutputStatus       )
				DELETEPAYLOADCASE(EventType::BinaryCommandEvent       )
				DELETEPAYLOADCASE(EventType::AnalogCommandEvent       )
				DELETEPAYLOADCASE(EventType::OctetString              )
				DELETEPAYLOADCASE(EventType::TimeAndInterval          )
				DELETEPAYLOADCASE(EventType::SecurityStat             )
				DELETEPAYLOADCASE(EventType::ControlRelayOutputBlock  )
				DELETEPAYLOADCASE(EventType::AnalogOutputInt16        )
				DELETEPAYLOADCASE(EventType::AnalogOutputInt32        )
				DELETEPAYLOADCASE(EventType::AnalogOutputFloat32      )
				DELETEPAYLOADCASE(EventType::AnalogOutputDouble64     )
				DELETEPAYLOADCASE(EventType::BinaryQuality            )
				DELETEPAYLOADCASE(EventType::DoubleBitBinaryQuality   )
				DELETEPAYLOADCASE(EventType::AnalogQuality            )
				DELETEPAYLOADCASE(EventType::CounterQuality           )
				DELETEPAYLOADCASE(EventType::BinaryOutputStatusQuality)
				DELETEPAYLOADCASE(EventType::FrozenCounterQuality     )
				DELETEPAYLOADCASE(EventType::AnalogOutputStatusQuality)
				DELETEPAYLOADCASE(EventType::OctetStringQuality       )
				DELETEPAYLOADCASE(EventType::FileAuth                 )
				DELETEPAYLOADCASE(EventType::FileCommand              )
				DELETEPAYLOADCASE(EventType::FileCommandStatus        )
				DELETEPAYLOADCASE(EventType::FileTransport            )
				DELETEPAYLOADCASE(EventType::FileTransportStatus      )
				DELETEPAYLOADCASE(EventType::FileDescriptor           )
				DELETEPAYLOADCASE(EventType::FileSpecString           )
				DELETEPAYLOADCASE(EventType::ConnectState             )
				DELETEPAYLOADCASE(EventType::Reserved1                )
				DELETEPAYLOADCASE(EventType::Reserved2                )
				DELETEPAYLOADCASE(EventType::Reserved3                )
				DELETEPAYLOADCASE(EventType::Reserved4                )
				DELETEPAYLOADCASE(EventType::Reserved5                )
				DELETEPAYLOADCASE(EventType::Reserved6                )
				DELETEPAYLOADCASE(EventType::Reserved8                )
				DELETEPAYLOADCASE(EventType::Reserved9                )
				DELETEPAYLOADCASE(EventType::Reserved10               )
				DELETEPAYLOADCASE(EventType::Reserved11               )
				DELETEPAYLOADCASE(EventType::Reserved12               )
				default:
					if(auto log = odc::spdlog_get("opendatacon"))
					{
						log->critical("odc::EventInfo destructor can't handle EventType::{}. Terminating",ToString(Type));
						log->flush();
					}
					//Can't throw from destructor - so exit
					exit(1);
			}
		}
	}

	//Getters
	const EventType& GetEventType() const { return Type; }
	const size_t& GetIndex() const { return Index; }
	const msSinceEpoch_t& GetTimestamp() const { return Timestamp; }
	const QualityFlags& GetQuality() const { return Quality; }
	const std::string& GetSourcePort() const { return SourcePort; }
	bool HasPayload() const { return pPayload; }

	template<EventType t>
	const typename EventTypePayload<t>::type& GetPayload() const
	{
		if(t != Type)
			throw std::logic_error("Wrong payload type requested for selected odc::EventInfo");
		if(!pPayload)
			throw std::runtime_error("Called GetPayload on uninitialised odc::EventInfo payload");
		return *static_cast<typename EventTypePayload<t>::type*>(pPayload);
	}

	std::string GetPayloadString(DataToStringMethod D2S = DataToStringMethod::Hex) const
	{
		switch(Type)
		{
			case EventType::ConnectState:
				return ToString(GetPayload<EventType::ConnectState>());
			case EventType::Binary:
				return std::to_string(GetPayload<EventType::Binary>());
			case EventType::Analog:
				return std::to_string(GetPayload<EventType::Analog>());
			case EventType::Counter:
				return std::to_string(GetPayload<EventType::Counter>());
			case EventType::FrozenCounter:
				return std::to_string(GetPayload<EventType::FrozenCounter>());
			case EventType::BinaryOutputStatus:
				return std::to_string(GetPayload<EventType::BinaryOutputStatus>());
			case EventType::AnalogOutputStatus:
				return std::to_string(GetPayload<EventType::AnalogOutputStatus>());
			case EventType::ControlRelayOutputBlock:
				return std::string(GetPayload<EventType::ControlRelayOutputBlock>());
			case EventType::AnalogOutputInt16:
				return std::to_string(GetPayload<EventType::AnalogOutputInt16>().first);
			case EventType::AnalogOutputInt32:
				return std::to_string(GetPayload<EventType::AnalogOutputInt32>().first);
			case EventType::AnalogOutputFloat32:
				return std::to_string(GetPayload<EventType::AnalogOutputFloat32>().first);
			case EventType::AnalogOutputDouble64:
				return std::to_string(GetPayload<EventType::AnalogOutputDouble64>().first);
			case EventType::OctetString:
				return ToString(GetPayload<EventType::OctetString>(),D2S);
			case EventType::BinaryQuality:
				return ToString(GetPayload<EventType::BinaryQuality>());
			case EventType::DoubleBitBinaryQuality:
				return ToString(GetPayload<EventType::DoubleBitBinaryQuality>());
			case EventType::AnalogQuality:
				return ToString(GetPayload<EventType::AnalogQuality>());
			case EventType::CounterQuality:
				return ToString(GetPayload<EventType::CounterQuality>());
			case EventType::BinaryOutputStatusQuality:
				return ToString(GetPayload<EventType::BinaryOutputStatusQuality>());
			case EventType::FrozenCounterQuality:
				return ToString(GetPayload<EventType::FrozenCounterQuality>());
			case EventType::AnalogOutputStatusQuality:
				return ToString(GetPayload<EventType::AnalogOutputStatusQuality>());
			default:
				return "<no_string_representation>";
		}
	}

	//Setters
	void SetIndex(size_t i){ Index = i; }
	void SetTimestamp(msSinceEpoch_t tm = msSinceEpoch()){ Timestamp = tm; }
	void SetQuality(QualityFlags q){ Quality = q; }
	void SetSource(const std::string& s){ SourcePort = s; }

	template<EventType t>
	void SetPayload(typename EventTypePayload<t>::type&& p)
	{
		if(t != Type)
			throw std::runtime_error("Wrong payload type specified for selected odc::EventInfo");
		if(pPayload)
			delete static_cast<typename EventTypePayload<t>::type*>(pPayload);
		pPayload = new typename EventTypePayload<t>::type(std::move(p));
	}

	void SetPayload(const std::string& PayloadStr, DataToStringMethod D2S = DataToStringMethod::Raw)
	{
		switch(Type)
		{
			STRINGPAYLOADCASE(EventType::Binary                   )
			STRINGPAYLOADCASE(EventType::DoubleBitBinary          )
			STRINGPAYLOADCASE(EventType::Analog                   )
			STRINGPAYLOADCASE(EventType::Counter                  )
			STRINGPAYLOADCASE(EventType::FrozenCounter            )
			STRINGPAYLOADCASE(EventType::BinaryOutputStatus       )
			STRINGPAYLOADCASE(EventType::AnalogOutputStatus       )
			STRINGPAYLOADCASE(EventType::BinaryCommandEvent       )
			STRINGPAYLOADCASE(EventType::AnalogCommandEvent       )
			STRINGPAYLOADCASE(EventType::TimeAndInterval          )
			STRINGPAYLOADCASE(EventType::SecurityStat             )
			STRINGPAYLOADCASE(EventType::ControlRelayOutputBlock  )
			STRINGPAYLOADCASE(EventType::AnalogOutputInt16        )
			STRINGPAYLOADCASE(EventType::AnalogOutputInt32        )
			STRINGPAYLOADCASE(EventType::AnalogOutputFloat32      )
			STRINGPAYLOADCASE(EventType::AnalogOutputDouble64     )
			STRINGPAYLOADCASE(EventType::BinaryQuality            )
			STRINGPAYLOADCASE(EventType::DoubleBitBinaryQuality   )
			STRINGPAYLOADCASE(EventType::AnalogQuality            )
			STRINGPAYLOADCASE(EventType::CounterQuality           )
			STRINGPAYLOADCASE(EventType::BinaryOutputStatusQuality)
			STRINGPAYLOADCASE(EventType::FrozenCounterQuality     )
			STRINGPAYLOADCASE(EventType::AnalogOutputStatusQuality)
			STRINGPAYLOADCASE(EventType::OctetStringQuality       )
			STRINGPAYLOADCASE(EventType::FileAuth                 )
			STRINGPAYLOADCASE(EventType::FileCommand              )
			STRINGPAYLOADCASE(EventType::FileCommandStatus        )
			STRINGPAYLOADCASE(EventType::FileTransport            )
			STRINGPAYLOADCASE(EventType::FileTransportStatus      )
			STRINGPAYLOADCASE(EventType::FileDescriptor           )
			STRINGPAYLOADCASE(EventType::FileSpecString           )
			STRINGPAYLOADCASE(EventType::ConnectState             )
			STRINGPAYLOADCASE(EventType::Reserved1                )
			STRINGPAYLOADCASE(EventType::Reserved2                )
			STRINGPAYLOADCASE(EventType::Reserved3                )
			STRINGPAYLOADCASE(EventType::Reserved4                )
			STRINGPAYLOADCASE(EventType::Reserved5                )
			STRINGPAYLOADCASE(EventType::Reserved6                )
			STRINGPAYLOADCASE(EventType::Reserved8                )
			STRINGPAYLOADCASE(EventType::Reserved9                )
			STRINGPAYLOADCASE(EventType::Reserved10               )
			STRINGPAYLOADCASE(EventType::Reserved11               )
			STRINGPAYLOADCASE(EventType::Reserved12               )
			case EventType::OctetString:
				SetPayload<EventType::OctetString>(PayloadFromString(PayloadStr,D2S));
				break;
			default:
				std::string msg = "odc::EventInfo payload from string setter can't handle EventType::"+ToString(Type);
				throw std::runtime_error(msg);
				break;
		}
	}

	//Set default payload - mostly for testing
	void SetPayload()
	{
		if(pPayload)
			return;
		switch(Type)
		{
			DEFAULTPAYLOADCASE(EventType::Binary                   )
			DEFAULTPAYLOADCASE(EventType::DoubleBitBinary          )
			DEFAULTPAYLOADCASE(EventType::Analog                   )
			DEFAULTPAYLOADCASE(EventType::Counter                  )
			DEFAULTPAYLOADCASE(EventType::FrozenCounter            )
			DEFAULTPAYLOADCASE(EventType::BinaryOutputStatus       )
			DEFAULTPAYLOADCASE(EventType::AnalogOutputStatus       )
			DEFAULTPAYLOADCASE(EventType::BinaryCommandEvent       )
			DEFAULTPAYLOADCASE(EventType::AnalogCommandEvent       )
			DEFAULTPAYLOADCASE(EventType::OctetString              )
			DEFAULTPAYLOADCASE(EventType::TimeAndInterval          )
			DEFAULTPAYLOADCASE(EventType::SecurityStat             )
			DEFAULTPAYLOADCASE(EventType::ControlRelayOutputBlock  )
			DEFAULTPAYLOADCASE(EventType::AnalogOutputInt16        )
			DEFAULTPAYLOADCASE(EventType::AnalogOutputInt32        )
			DEFAULTPAYLOADCASE(EventType::AnalogOutputFloat32      )
			DEFAULTPAYLOADCASE(EventType::AnalogOutputDouble64     )
			DEFAULTPAYLOADCASE(EventType::BinaryQuality            )
			DEFAULTPAYLOADCASE(EventType::DoubleBitBinaryQuality   )
			DEFAULTPAYLOADCASE(EventType::AnalogQuality            )
			DEFAULTPAYLOADCASE(EventType::CounterQuality           )
			DEFAULTPAYLOADCASE(EventType::BinaryOutputStatusQuality)
			DEFAULTPAYLOADCASE(EventType::FrozenCounterQuality     )
			DEFAULTPAYLOADCASE(EventType::AnalogOutputStatusQuality)
			DEFAULTPAYLOADCASE(EventType::OctetStringQuality       )
			DEFAULTPAYLOADCASE(EventType::FileAuth                 )
			DEFAULTPAYLOADCASE(EventType::FileCommand              )
			DEFAULTPAYLOADCASE(EventType::FileCommandStatus        )
			DEFAULTPAYLOADCASE(EventType::FileTransport            )
			DEFAULTPAYLOADCASE(EventType::FileTransportStatus      )
			DEFAULTPAYLOADCASE(EventType::FileDescriptor           )
			DEFAULTPAYLOADCASE(EventType::FileSpecString           )
			DEFAULTPAYLOADCASE(EventType::ConnectState             )
			DEFAULTPAYLOADCASE(EventType::Reserved1                )
			DEFAULTPAYLOADCASE(EventType::Reserved2                )
			DEFAULTPAYLOADCASE(EventType::Reserved3                )
			DEFAULTPAYLOADCASE(EventType::Reserved4                )
			DEFAULTPAYLOADCASE(EventType::Reserved5                )
			DEFAULTPAYLOADCASE(EventType::Reserved6                )
			DEFAULTPAYLOADCASE(EventType::Reserved8                )
			DEFAULTPAYLOADCASE(EventType::Reserved9                )
			DEFAULTPAYLOADCASE(EventType::Reserved10               )
			DEFAULTPAYLOADCASE(EventType::Reserved11               )
			DEFAULTPAYLOADCASE(EventType::Reserved12               )
			default:
				std::string msg = "odc::EventInfo default payload setter can't handle EventType::"+ToString(Type);
				throw std::runtime_error(msg);
				break;
		}
	}

private:
	size_t Index;
	msSinceEpoch_t Timestamp;
	QualityFlags Quality;
	std::string SourcePort;
	const EventType Type;
	void *pPayload;
};

inline bool operator==(const odc::EventInfo& lhs, const odc::EventInfo& rhs)
{
	if (&lhs == &rhs) return true;
	if (lhs.GetTimestamp() != rhs.GetTimestamp()) return false;
	if (lhs.GetQuality() != rhs.GetQuality()) return false;
	if (lhs.GetEventType() != rhs.GetEventType()) return false;
	if (lhs.GetIndex() != rhs.GetIndex()) return false;
	if (lhs.HasPayload() != rhs.HasPayload()) return false;

	bool same_payload = [&]()
				  {
					  if (lhs.HasPayload())
					  {
						  switch (lhs.GetEventType())
						  {
							  case odc::EventType::Binary:
								  return lhs.GetPayload<odc::EventType::Binary>() == rhs.GetPayload<odc::EventType::Binary>();
							  case odc::EventType::DoubleBitBinary:
								  return lhs.GetPayload<odc::EventType::DoubleBitBinary>() == rhs.GetPayload<odc::EventType::DoubleBitBinary>();
							  case odc::EventType::Analog:
								  return lhs.GetPayload<odc::EventType::Analog>() == rhs.GetPayload<odc::EventType::Analog>();
							  case odc::EventType::Counter:
								  return lhs.GetPayload<odc::EventType::Counter>() == rhs.GetPayload<odc::EventType::Counter>();
							  case odc::EventType::FrozenCounter:
								  return lhs.GetPayload<odc::EventType::FrozenCounter>() == rhs.GetPayload<odc::EventType::FrozenCounter>();
							  case odc::EventType::BinaryOutputStatus:
								  return lhs.GetPayload<odc::EventType::BinaryOutputStatus>() == rhs.GetPayload<odc::EventType::BinaryOutputStatus>();
							  case odc::EventType::AnalogOutputStatus:
								  return lhs.GetPayload<odc::EventType::AnalogOutputStatus>() == rhs.GetPayload<odc::EventType::AnalogOutputStatus>();
							  case odc::EventType::BinaryCommandEvent:
								  return lhs.GetPayload<odc::EventType::BinaryCommandEvent>() == rhs.GetPayload<odc::EventType::BinaryCommandEvent>();
							  case odc::EventType::AnalogCommandEvent:
								  return lhs.GetPayload<odc::EventType::AnalogCommandEvent>() == rhs.GetPayload<odc::EventType::AnalogCommandEvent>();
							  case odc::EventType::OctetString:
								  return lhs.GetPayload<odc::EventType::OctetString>() == rhs.GetPayload<odc::EventType::OctetString>();
							  case odc::EventType::TimeAndInterval:
								  return lhs.GetPayload<odc::EventType::TimeAndInterval>() == rhs.GetPayload<odc::EventType::TimeAndInterval>();
							  case odc::EventType::SecurityStat:
								  return lhs.GetPayload<odc::EventType::SecurityStat>() == rhs.GetPayload<odc::EventType::SecurityStat>();
							  case odc::EventType::ControlRelayOutputBlock:
								  return lhs.GetPayload<odc::EventType::ControlRelayOutputBlock>() == rhs.GetPayload<odc::EventType::ControlRelayOutputBlock>();
							  case odc::EventType::AnalogOutputInt16:
								  return lhs.GetPayload<odc::EventType::AnalogOutputInt16>() == rhs.GetPayload<odc::EventType::AnalogOutputInt16>();
							  case odc::EventType::AnalogOutputInt32:
								  return lhs.GetPayload<odc::EventType::AnalogOutputInt32>() == rhs.GetPayload<odc::EventType::AnalogOutputInt32>();
							  case odc::EventType::AnalogOutputFloat32:
								  return lhs.GetPayload<odc::EventType::AnalogOutputFloat32>() == rhs.GetPayload<odc::EventType::AnalogOutputFloat32>();
							  case odc::EventType::AnalogOutputDouble64:
								  return lhs.GetPayload<odc::EventType::AnalogOutputDouble64>() == rhs.GetPayload<odc::EventType::AnalogOutputDouble64>();
							  case odc::EventType::BinaryQuality:
								  return lhs.GetPayload<odc::EventType::BinaryQuality>() == rhs.GetPayload<odc::EventType::BinaryQuality>();
							  case odc::EventType::DoubleBitBinaryQuality:
								  return lhs.GetPayload<odc::EventType::DoubleBitBinaryQuality>() == rhs.GetPayload<odc::EventType::DoubleBitBinaryQuality>();
							  case odc::EventType::AnalogQuality:
								  return lhs.GetPayload<odc::EventType::AnalogQuality>() == rhs.GetPayload<odc::EventType::AnalogQuality>();
							  case odc::EventType::CounterQuality:
								  return lhs.GetPayload<odc::EventType::CounterQuality>() == rhs.GetPayload<odc::EventType::CounterQuality>();
							  case odc::EventType::BinaryOutputStatusQuality:
								  return lhs.GetPayload<odc::EventType::BinaryOutputStatusQuality>() == rhs.GetPayload<odc::EventType::BinaryOutputStatusQuality>();
							  case odc::EventType::FrozenCounterQuality:
								  return lhs.GetPayload<odc::EventType::FrozenCounterQuality>() == rhs.GetPayload<odc::EventType::FrozenCounterQuality>();
							  case odc::EventType::AnalogOutputStatusQuality:
								  return lhs.GetPayload<odc::EventType::AnalogOutputStatusQuality>() == rhs.GetPayload<odc::EventType::AnalogOutputStatusQuality>();
							  case odc::EventType::OctetStringQuality:
								  return lhs.GetPayload<odc::EventType::OctetStringQuality>() == rhs.GetPayload<odc::EventType::OctetStringQuality>();
							  case odc::EventType::ConnectState:
								  return lhs.GetPayload<odc::EventType::ConnectState>() == rhs.GetPayload<odc::EventType::ConnectState>();
							  default:
							  {
								  if(auto log = odc::spdlog_get("opendatacon"))
									  log->error("odc::EventInfo comparison can't handle EventType::{}. Returning unequal", ToString(lhs.GetEventType()));
								  return false;
							  }
						  }
					  }
					  return true; // Both have no payloads
				  }();
	if(!same_payload) return false;

	//compare source port last (only if needed) since string compare ain't cheap
	if (lhs.GetSourcePort() != rhs.GetSourcePort()) return false;

	return true;
}

inline bool operator!=(const odc::EventInfo& lhs, const odc::EventInfo& rhs)
{
	return !(lhs == rhs);
}

}

#endif
