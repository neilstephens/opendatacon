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
 * MmsNames.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Symbolic names of MMS error codes, see MmsNames.h.
 */

#include "MmsNames.h"
#include <initializer_list>

namespace iccp
{
namespace mms
{

namespace
{
struct Named
{
	int64_t value;
	const char* name;
};

template<size_t N>
const char* Lookup(const Named (&table)[N], int64_t value)
{
	for(const auto& n : table)
		if(n.value == value)
			return n.name;
	return nullptr;
}

std::string WithNumber(const char* name, int64_t value)
{
	return std::string(name ? name : "unknown") + " [" + std::to_string(value) + "]";
}

//ServiceError.errorClass (ISO 9506-2 ServiceError). Verified: none against a capture. Least certain: the file class.
const Named classNames[] = {
	{0, "vmd-state"}, {1, "application-reference"}, {2, "definition"}, {3, "resource"}, {4, "service"},
	{5, "service-preempt"}, {6, "time-resolution"}, {7, "access"}, {8, "initiate"}, {9, "conclude"}, {10, "cancel"}, {11, "file"}, {12, "other"}
};

//the per class codes. "other" is 0 in every class. UNVERIFIED against the standard text: service-preempt, initiate, file.
const Named vmdState[] = {
	{0, "other"}, {1, "vmd-state-conflict"}, {2, "vmd-operational-problem"}, {3, "domain-transfer-problem"}, {4, "state-machine-id-invalid"}
};
const Named applicationReference[] = {
	{0, "other"}, {1, "application-unreachable"}, {2, "connection-lost"}, {3, "application-reference-invalid"}, {4, "context-unsupported"}
};
const Named definition[] = {
	{0, "other"}, {1, "object-undefined"}, {2, "invalid-address"}, {3, "type-unsupported"}, {4, "type-inconsistent"},
	{5, "object-exists"}, {6, "object-attribute-inconsistent"}
};
const Named resource[] = {
	{0, "other"}, {1, "memory-unavailable"}, {2, "processor-resource-unavailable"}, {3, "mass-storage-unavailable"},
	{4, "capability-unavailable"}, {5, "capability-unknown"}
};
const Named service[] = {
	{0, "other"}, {1, "primitives-out-of-sequence"}, {2, "object-state-conflict"}, {3, "pdu-size"}, {4, "continuation-invalid"},
	{5, "object-constraint-conflict"}
};
const Named servicePreempt[] = {
	{0, "other"}, {1, "timeout"}, {2, "deadlock"}, {3, "cancel"}
}; //unverified
const Named timeResolution[] = {
	{0, "other"}, {1, "unsupportable-time-resolution"}
};
const Named access[] = {
	{0, "other"}, {1, "object-access-unsupported"}, {2, "object-non-existent"}, {3, "object-access-denied"}, {4, "object-invalidated"}
};
const Named initiate[] = {
	{0, "other"}, {1, "version-incompatible"}, {2, "max-segment-insufficient"}, {3, "max-services-outstanding-calling-insufficient"},
	{4, "max-services-outstanding-called-insufficient"}, {5, "service-CBB-insufficient"}, {6, "parameter-CBB-insufficient"}, {7, "nesting-level-insufficient"}
}; //unverified
const Named conclude[] = {
	{0, "other"}, {1, "further-communication-required"}
};
const Named cancel[] = {
	{0, "other"}, {1, "invoke-id-unknown"}, {2, "cancel-not-possible"}
};
const Named file[] = {
	{0, "other"}, {1, "filename-ambiguous"}, {2, "file-busy"}, {3, "filename-syntax-error"}, {4, "content-type-invalid"},
	{5, "position-invalid"}, {6, "file-access-denied"}, {7, "file-non-existent"}, {8, "duplicate-filename"}, {9, "insufficient-space-in-filestore"}
}; //unverified
const Named others[] = {
	{0, "other"}
};

//DataAccessError (also the Mms.h DataAccessError constants)
const Named dataAccess[] = {
	{0, "object-invalidated"}, {1, "hardware-fault"}, {2, "temporarily-unavailable"}, {3, "object-access-denied"},
	{4, "object-undefined"}, {5, "invalid-address"}, {6, "type-unsupported"}, {7, "type-inconsistent"}, {8, "object-attribute-inconsistent"},
	{9, "object-access-unsupported"}, {10, "object-non-existent"}, {11, "object-value-invalid"}
};

//Reject: the type is the alternative of rejectReason, the reason the code within it. UNVERIFIED against the standard text and captures:
//the code tables below are from memory of the ISO 9506-2 RejectPDU.
const Named rejectTypes[] = {
	{1, "confirmed-requestPDU"}, {2, "confirmed-responsePDU"}, {3, "confirmed-errorPDU"}, {4, "unconfirmedPDU"}, {5, "pdu-error"},
	{6, "cancel-requestPDU"}, {7, "cancel-responsePDU"}, {8, "cancel-errorPDU"}, {9, "conclude-requestPDU"}, {10, "conclude-responsePDU"}, {11, "conclude-errorPDU"}
};
const Named rejectConfirmedRequest[] = {
	{0, "other"}, {1, "unrecognized-service"}, {2, "unrecognized-modifier"}, {3, "invalid-invokeID"}, {4, "invalid-argument"},
	{5, "invalid-modifier"}, {6, "max-serv-outstanding-exceeded"}, {8, "max-recursion-exceeded"}, {9, "value-out-of-range"}
};
const Named rejectConfirmedResponse[] = {
	{0, "other"}, {1, "unrecognized-service"}, {2, "invalid-invokeID"}, {3, "invalid-result"}, {5, "max-recursion-exceeded"},
	{6, "value-out-of-range"}
};
const Named rejectConfirmedError[] = {
	{0, "other"}, {1, "unrecognized-service"}, {2, "invalid-invokeID"}, {3, "invalid-serviceError"}, {4, "value-out-of-range"}
};
const Named rejectUnconfirmed[] = {
	{0, "other"}, {1, "unrecognized-service"}, {2, "invalid-argument"}
};
const Named rejectPduError[] = {
	{0, "unknown-pdu-type"}, {1, "invalid-pdu"}, {2, "illegal-acse-mapping"}
};
const Named rejectCancelRequest[] = {
	{0, "other"}, {1, "invalid-invokeID"}
};
const Named rejectCancelError[] = {
	{0, "other"}, {1, "invalid-invokeID"}, {2, "invalid-serviceError"}, {3, "value-out-of-range"}
};
const Named rejectConcludeRequest[] = {
	{0, "other"}, {1, "invalid-argument"}
};
const Named rejectConcludeResponse[] = {
	{0, "other"}, {1, "invalid-result"}
};
const Named rejectConcludeError[] = {
	{0, "other"}, {1, "invalid-serviceError"}, {2, "value-out-of-range"}
};
}

const char* ErrorClassName(int64_t errorClass)
{
	return Lookup(classNames, errorClass);
}

const char* ErrorCodeName(int64_t errorClass, int64_t errorCode)
{
	switch(errorClass)
	{
		case ErrorClass::VmdState: return Lookup(vmdState, errorCode);
		case ErrorClass::ApplicationReference: return Lookup(applicationReference, errorCode);
		case ErrorClass::Definition: return Lookup(definition, errorCode);
		case ErrorClass::Resource: return Lookup(resource, errorCode);
		case ErrorClass::Service: return Lookup(service, errorCode);
		case ErrorClass::ServicePreempt: return Lookup(servicePreempt, errorCode);
		case ErrorClass::TimeResolution: return Lookup(timeResolution, errorCode);
		case ErrorClass::Access: return Lookup(access, errorCode);
		case ErrorClass::Initiate: return Lookup(initiate, errorCode);
		case ErrorClass::Conclude: return Lookup(conclude, errorCode);
		case ErrorClass::Cancel: return Lookup(cancel, errorCode);
		case ErrorClass::File: return Lookup(file, errorCode);
		case ErrorClass::Others: return Lookup(others, errorCode);
		default: return nullptr;
	}
}

const char* DataAccessErrorName(int64_t error)
{
	return Lookup(dataAccess, error);
}

const char* RejectTypeName(int64_t type)
{
	return Lookup(rejectTypes, type);
}

const char* RejectReasonName(int64_t type, int64_t reason)
{
	switch(type)
	{
		case RejectType::ConfirmedRequestPdu: return Lookup(rejectConfirmedRequest, reason);
		case RejectType::ConfirmedResponsePdu: return Lookup(rejectConfirmedResponse, reason);
		case RejectType::ConfirmedErrorPdu: return Lookup(rejectConfirmedError, reason);
		case RejectType::UnconfirmedPdu: return Lookup(rejectUnconfirmed, reason);
		case RejectType::PduError: return Lookup(rejectPduError, reason);
		case RejectType::CancelRequestPdu:
		case RejectType::CancelResponsePdu: return Lookup(rejectCancelRequest, reason);
		case RejectType::CancelErrorPdu: return Lookup(rejectCancelError, reason);
		case RejectType::ConcludeRequestPdu: return Lookup(rejectConcludeRequest, reason);
		case RejectType::ConcludeResponsePdu: return Lookup(rejectConcludeResponse, reason);
		case RejectType::ConcludeErrorPdu: return Lookup(rejectConcludeError, reason);
		default: return nullptr;
	}
}

std::string Describe(const ServiceError& error)
{
	std::string s = WithNumber(ErrorClassName(error.errorClass), error.errorClass) + " / " +
	                WithNumber(ErrorCodeName(error.errorClass, error.errorCode), error.errorCode);
	if(error.additionalCode)
		s += ", additional code " + std::to_string(*error.additionalCode);
	if(error.additionalDescription)
	{
		//the text came from the peer: keep it to one printable line
		std::string d;
		for(char c : *error.additionalDescription)
			d.push_back((c >= 0x20 && c < 0x7F) ? c : '?');
		if(d.size() > 200)
			d.resize(200);
		s += ", additional description \"" + d + "\"";
	}
	return s;
}

std::string DescribeDataAccessError(int64_t error)
{
	return WithNumber(DataAccessErrorName(error), error);
}

std::string Describe(const Reject& reject)
{
	std::string s = WithNumber(RejectTypeName(reject.type), reject.type) + " / " + WithNumber(RejectReasonName(reject.type, reject.reason), reject.reason);
	if(reject.originalInvokeId)
		s += ", invoke id " + std::to_string(*reject.originalInvokeId);
	return s;
}

} // namespace mms
} // namespace iccp
