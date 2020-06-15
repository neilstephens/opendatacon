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

#include <chrono>
#include <string>
#include <tuple>
#include <opendatacon/EnumClassFlags.h>
#include <opendatacon/util.h>

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

	Reserved7                 = 31,
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
	Reserved12                = 43,

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
	NUL_CANCEL = 2,
	PULSE_ON = 3,
	PULSE_ON_CANCEL = 4,
	PULSE_OFF = 5,
	PULSE_OFF_CANCEL = 6,
	LATCH_ON = 7,
	LATCH_ON_CANCEL = 8,
	LATCH_OFF = 9,
	LATCH_OFF_CANCEL = 10,
	CLOSE_PULSE_ON = 11,
	CLOSE_PULSE_ON_CANCEL = 12,
	TRIP_PULSE_ON = 13,
	TRIP_PULSE_ON_CANCEL = 14,
	UNDEFINED = 15
};

//TODO: make these ToString functions faster
//	use hash map cache
#define ENUMSTRING(A,E,B) if(A == E::B) return #B;

inline std::string ToString(const CommandStatus cc)
{
	ENUMSTRING(cc, CommandStatus,SUCCESS )
	ENUMSTRING(cc, CommandStatus,TIMEOUT)
	ENUMSTRING(cc, CommandStatus,NO_SELECT)
	ENUMSTRING(cc, CommandStatus,FORMAT_ERROR)
	ENUMSTRING(cc, CommandStatus,NOT_SUPPORTED)
	ENUMSTRING(cc, CommandStatus,ALREADY_ACTIVE)
	ENUMSTRING(cc, CommandStatus,HARDWARE_ERROR)
	ENUMSTRING(cc, CommandStatus,LOCAL)
	ENUMSTRING(cc, CommandStatus,TOO_MANY_OPS)
	ENUMSTRING(cc, CommandStatus,NOT_AUTHORIZED)
	ENUMSTRING(cc, CommandStatus,AUTOMATION_INHIBIT)
	ENUMSTRING(cc, CommandStatus,PROCESSING_LIMITED)
	ENUMSTRING(cc, CommandStatus,OUT_OF_RANGE)
	ENUMSTRING(cc, CommandStatus,DOWNSTREAM_LOCAL)
	ENUMSTRING(cc, CommandStatus,ALREADY_COMPLETE)
	ENUMSTRING(cc, CommandStatus,BLOCKED)
	ENUMSTRING(cc, CommandStatus,CANCELLED)
	ENUMSTRING(cc, CommandStatus,BLOCKED_OTHER_MASTER)
	ENUMSTRING(cc, CommandStatus,DOWNSTREAM_FAIL)
	ENUMSTRING(cc, CommandStatus,NON_PARTICIPATING)
	ENUMSTRING(cc, CommandStatus,UNDEFINED)
	return "<no_string_representation>";
}

inline std::string ToString(const ControlCode cc)
{
	ENUMSTRING(cc,ControlCode,NUL                  )
	ENUMSTRING(cc,ControlCode,NUL_CANCEL           )
	ENUMSTRING(cc,ControlCode,PULSE_ON             )
	ENUMSTRING(cc,ControlCode,PULSE_ON_CANCEL      )
	ENUMSTRING(cc,ControlCode,PULSE_OFF            )
	ENUMSTRING(cc,ControlCode,PULSE_OFF_CANCEL     )
	ENUMSTRING(cc,ControlCode,LATCH_ON             )
	ENUMSTRING(cc,ControlCode,LATCH_ON_CANCEL      )
	ENUMSTRING(cc,ControlCode,LATCH_OFF            )
	ENUMSTRING(cc,ControlCode,LATCH_OFF_CANCEL     )
	ENUMSTRING(cc,ControlCode,CLOSE_PULSE_ON       )
	ENUMSTRING(cc,ControlCode,CLOSE_PULSE_ON_CANCEL)
	ENUMSTRING(cc,ControlCode,TRIP_PULSE_ON        )
	ENUMSTRING(cc,ControlCode,TRIP_PULSE_ON_CANCEL )
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
	ENUMSTRING(et,EventType,Reserved7                )
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

enum class ConnectState {PORT_UP,CONNECTED,DISCONNECTED,PORT_DOWN};
inline std::string ToString(const ConnectState cs)
{
	ENUMSTRING(cs, ConnectState, PORT_UP)
	ENUMSTRING(cs, ConnectState, CONNECTED)
	ENUMSTRING(cs, ConnectState, DISCONNECTED)
	ENUMSTRING(cs, ConnectState, PORT_DOWN)
	return "<no_string_representation>";
}

inline bool GetQualityFlagsFromStringName(const std::string StrQuality, QualityFlags& QualityResult)
{
#define CHECKFLAGSTRING(X) if (StrQuality.find(#X) != std::string::npos) QualityResult |= QualityFlags::X

	QualityResult = QualityFlags::NONE;

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

	return (QualityResult != QualityFlags::NONE); // Should never be none!
}

inline bool GetEventTypeFromStringName(const std::string StrEventType, EventType& EventTypeResult)
{
#define CHECKEVENTSTRING(X) if (StrEventType.find(ToString(X)) != std::string::npos) EventTypeResult = X

	EventTypeResult = EventType::BeforeRange;

	CHECKEVENTSTRING(EventType::ConnectState);
	CHECKEVENTSTRING(EventType::Binary);
	CHECKEVENTSTRING(EventType::Analog);
	CHECKEVENTSTRING(EventType::Counter);
	CHECKEVENTSTRING(EventType::FrozenCounter);
	CHECKEVENTSTRING(EventType::BinaryOutputStatus);
	CHECKEVENTSTRING(EventType::AnalogOutputStatus);
	CHECKEVENTSTRING(EventType::ControlRelayOutputBlock);
	CHECKEVENTSTRING(EventType::OctetString);
	CHECKEVENTSTRING(EventType::BinaryQuality);
	CHECKEVENTSTRING(EventType::DoubleBitBinaryQuality);
	CHECKEVENTSTRING(EventType::AnalogQuality);
	CHECKEVENTSTRING(EventType::CounterQuality);
	CHECKEVENTSTRING(EventType::BinaryOutputStatusQuality);
	CHECKEVENTSTRING( EventType::FrozenCounterQuality);
	CHECKEVENTSTRING(EventType::AnalogOutputStatusQuality);

	return (EventTypeResult != EventType::BeforeRange);
}
inline bool GetControlCodeFromStringName(const std::string StrControlCode, ControlCode& ControlCodeResult)
{
#define CHECKCONTROLCODESTRING(X) if (StrControlCode.find(ToString(X)) != std::string::npos) ControlCodeResult = X

	ControlCodeResult = ControlCode::UNDEFINED;

	CHECKCONTROLCODESTRING(ControlCode::CLOSE_PULSE_ON);
	CHECKCONTROLCODESTRING(ControlCode::CLOSE_PULSE_ON_CANCEL);
	CHECKCONTROLCODESTRING(ControlCode::LATCH_OFF);
	CHECKCONTROLCODESTRING(ControlCode::LATCH_OFF_CANCEL);
	CHECKCONTROLCODESTRING(ControlCode::LATCH_ON);
	CHECKCONTROLCODESTRING(ControlCode::LATCH_ON_CANCEL);
	CHECKCONTROLCODESTRING(ControlCode::NUL);
	CHECKCONTROLCODESTRING(ControlCode::NUL_CANCEL);
	CHECKCONTROLCODESTRING(ControlCode::PULSE_OFF);
	CHECKCONTROLCODESTRING(ControlCode::PULSE_OFF_CANCEL);
	CHECKCONTROLCODESTRING(ControlCode::PULSE_ON);
	CHECKCONTROLCODESTRING(ControlCode::PULSE_ON_CANCEL);
	CHECKCONTROLCODESTRING(ControlCode::TRIP_PULSE_ON);
	CHECKCONTROLCODESTRING(ControlCode::TRIP_PULSE_ON_CANCEL);

	return (ControlCodeResult != ControlCode::UNDEFINED);
}
inline bool GetConnectStateFromStringName(const std::string StrConnectState, ConnectState& ConnectStateResult)
{
#define CHECKCONNECTSTATESTRING(X) if (StrConnectState.find(ToString(X)) != std::string::npos) {ConnectStateResult = X;return true;}

	CHECKCONNECTSTATESTRING(ConnectState::CONNECTED);
	CHECKCONNECTSTATESTRING(ConnectState::DISCONNECTED);
	CHECKCONNECTSTATESTRING(ConnectState::PORT_DOWN);
	CHECKCONNECTSTATESTRING(ConnectState::PORT_UP);

	return false;
}

typedef uint64_t msSinceEpoch_t;
inline msSinceEpoch_t msSinceEpoch()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>
		       (std::chrono::system_clock::now().time_since_epoch()).count();
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

EVENTPAYLOAD(EventType::Binary                   , bool)
EVENTPAYLOAD(EventType::DoubleBitBinary          , DBB)
EVENTPAYLOAD(EventType::Analog                   , double)
EVENTPAYLOAD(EventType::Counter                  , uint32_t)
EVENTPAYLOAD(EventType::FrozenCounter            , uint32_t)
EVENTPAYLOAD(EventType::BinaryOutputStatus       , bool)
EVENTPAYLOAD(EventType::AnalogOutputStatus       , double)
EVENTPAYLOAD(EventType::BinaryCommandEvent       , CommandStatus)
EVENTPAYLOAD(EventType::AnalogCommandEvent       , CommandStatus)
EVENTPAYLOAD(EventType::OctetString              , std::string)
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
EVENTPAYLOAD(EventType::Reserved7                , char) //stub
EVENTPAYLOAD(EventType::Reserved8                , char) //stub
EVENTPAYLOAD(EventType::Reserved9                , char) //stub
EVENTPAYLOAD(EventType::Reserved10               , char) //stub
EVENTPAYLOAD(EventType::Reserved11               , char) //stub
EVENTPAYLOAD(EventType::Reserved12               , char) //stub
//TODO: map the rest

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
				COPYPAYLOADCASE(EventType::Reserved7                )
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
				DELETEPAYLOADCASE(EventType::Reserved7                )
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

	template<EventType t>
	const typename EventTypePayload<t>::type& GetPayload() const
	{
		if(t != Type)
			throw std::runtime_error("Wrong payload type requested for selected odc::EventInfo");
		if(!pPayload)
			throw std::runtime_error("Called GetPayload on uninitialised odc::EventInfo payload");
		return *static_cast<typename EventTypePayload<t>::type*>(pPayload);
	}

	std::string GetPayloadString() const
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
			case EventType::OctetString:
				return GetPayload<EventType::OctetString>();
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
			DEFAULTPAYLOADCASE(EventType::Reserved7                )
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

}

#endif
