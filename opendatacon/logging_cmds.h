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
 * logging_cmds.h
 *
 *  Created on: 14/07/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef LOGGING_CMDS_H_
#define LOGGING_CMDS_H_

#include <opendatacon/util.h>
#include "AdvancedLogger.h"
#include <sstream>

void cmd_ignore_message(std::stringstream& args, AdvancedLogger& AdvLog);
void cmd_unignore_message(std::stringstream& args, AdvancedLogger& AdvLog);
void cmd_show_ignored(std::stringstream& args, AdvancedLogger& AdvLog);

#endif /* LOGGING_CMDS_H_ */
