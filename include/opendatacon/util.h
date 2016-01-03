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

#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <map>
//#include <opendnp3/outstation/OutstationStackConfig.h>
//#include <opendnp3/outstation/DatabaseTemplate.h>
//#include <opendnp3/app/MeasurementTypes.h>
#include <cstdint>

//fast rough random numbers
#define CONG(jcong) (jcong = 69069*jcong+1234567)
#define ZERO_TO_ONE(a) (CONG(a)*2.328306e-10)
typedef uint32_t rand_t;

bool getline_noncomment(std::istream& is, std::string& line);
bool extract_delimited_string(std::istream& ist, std::string& extracted);
bool extract_delimited_string(const std::string& delims, std::istream& ist, std::string& extracted);

//bool operator== (opendnp3::OutstationStackConfig& SC1, opendnp3::OutstationStackConfig& SC2);
//bool operator!= (opendnp3::OutstationStackConfig& SC1, opendnp3::OutstationStackConfig& SC2);

bool GetBool(const std::string& value);

