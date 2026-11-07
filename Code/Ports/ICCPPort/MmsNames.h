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
 * MmsNames.h
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  Symbolic names of the MMS error, data access error and reject codes (ISO 9506-2), for diagnostics. The numbers
 *  are always kept in the text, in square brackets, so an unknown or mis-named code is still identifiable.
 *
 *  The names are from the ISO 9506-2 ASN.1 as remembered and from Wireshark's MMS dissector, not checked against the
 *  standard text: see the comments in MmsNames.cpp for the ones that are less certain.
 */

#ifndef ICCP_MMS_NAMES_H_
#define ICCP_MMS_NAMES_H_

#include "Mms.h"
#include <string>

namespace iccp
{
namespace mms
{

//nullptr if the class / code has no known name
const char* ErrorClassName(int64_t errorClass);
const char* ErrorCodeName(int64_t errorClass, int64_t errorCode);
const char* DataAccessErrorName(int64_t error);
const char* RejectTypeName(int64_t type);
const char* RejectReasonName(int64_t type, int64_t reason);

//"definition [2] / object-undefined [1]", then ", additional code N" and ", additional description \"...\"" if present
std::string Describe(const ServiceError& error);
//"object-non-existent [10]"
std::string DescribeDataAccessError(int64_t error);
//"confirmed-requestPDU [1] / unrecognized-service [1]" (plus the original invoke id if present)
std::string Describe(const Reject& reject);

} // namespace mms
} // namespace iccp

#endif /* ICCP_MMS_NAMES_H_ */
