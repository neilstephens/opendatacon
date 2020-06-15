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
 * util.cpp
 *
 *  Created on: 14/03/2014
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#include <iostream>
#include <opendatacon/util.h>
#include <regex>
#include <utility>
namespace odc
{

void spdlog_init_thread_pool(size_t q_size, size_t thread_count)
{
	spdlog::init_thread_pool(q_size, thread_count);
}

std::shared_ptr<spdlog::details::thread_pool> spdlog_thread_pool()
{
	return spdlog::thread_pool();
}

void spdlog_flush_all()
{
	spdlog::apply_all([&](const std::shared_ptr<spdlog::logger>& l) {l->flush(); });
}

void spdlog_apply_all(const std::function<void(std::shared_ptr<spdlog::logger>)> &fun)
{
	spdlog::apply_all(fun);
}

void spdlog_register_logger(std::shared_ptr<spdlog::logger> logger)
{
	return spdlog::register_logger(std::move(logger));
}

std::shared_ptr<spdlog::logger> spdlog_get(const std::string &name)
{
	return spdlog::get(name);
}

void spdlog_drop(const std::string &name)
{
	spdlog::drop(name);
}

void spdlog_drop_all()
{
	spdlog::drop_all();
}

void spdlog_shutdown()
{
	spdlog::shutdown();
}

bool getline_noncomment(std::istream& is, std::string& line)
{
	//chew up blank lines and comments
	do
	{
		std::getline(is, line);
	} while(std::regex_match(line,std::regex("^[:space:]*#.*|^[^_[:alnum:]]*$",std::regex::extended)) && !is.eof());

	//fail if we hit the end
	if(is.eof())
		return false;
	//success!
	return true;
}

bool extract_delimited_string(std::istream& ist, std::string& extracted)
{
	extracted.clear();
	char delim;
	//The first non-whitespace char is the delimiter
	ist>>std::ws; //eat whitspace
	if((delim = ist.peek()) == EOF)
		return false; //nothing to extract - return failed (no delimetered string)

	auto reset_pos = ist.tellg();
	size_t offset = 1;
	while(ist.seekg(1,std::ios_base::cur))
	{
		if(ist.peek() == delim)
		{
			ist.seekg(reset_pos);
			char ch = '\0';
			ist.get(ch); //start delim
			while(--offset)
			{
				ist.get(ch);
				extracted.push_back(ch);
			}
			ist.get(ch); //end delim
			return true;
		}
		offset++;
	}
	//if we get to here, something has gone wrong
	ist.seekg(reset_pos);
	return false;
}

bool extract_delimited_string(const std::string& delims, std::istream& ist, std::string& extracted)
{
	extracted.clear();
	char delim;
	//The first non-whitespace char is the delimiter
	ist>>std::ws; //eat whitspace
	if((delim = ist.peek()) == EOF)
		return false; //nothing to extract - return failed

	if(delims.find(delim) == std::string::npos)
	{
		/* not delimited so just extract until we get to a space or end of string */
		ist>>extracted;
		return true;
	}

	return extract_delimited_string(ist,extracted);
}

} // namespace odc
