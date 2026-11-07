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
 * NetAccess.cpp
 *
 *      Author: Neil Stephens <dearknarl@gmail.com>
 *
 *  See NetAccess.h
 */
#include "NetAccess.h"
#include <filesystem>
#include <system_error>

namespace
{
namespace fs = std::filesystem;

//the path with symbolic links and ".." resolved (as far as it exists)
fs::path Resolved(const std::string& p)
{
	std::error_code ec;
	auto r = fs::weakly_canonical(fs::absolute(fs::path(p),ec),ec);
	return ec ? fs::path() : r;
}

//is path the directory dir, or under it
bool Under(const fs::path& path, const fs::path& dir)
{
	auto d = dir.begin(), p = path.begin();
	for(; d != dir.end(); ++d, ++p)
	{
		if(d->empty()) //(a trailing separator)
			continue;
		if(p == path.end() || *p != *d)
			return false;
	}
	return true;
}

NetAccessDecision Error(const std::string& what)
{
	NetAccessDecision d;
	d.config_error = true;
	d.why = "\"NetEnabled\" "+what+": no network access";
	return d;
}
}

NetAccessDecision DecideNetAccess(const Json::Value& NetEnabled, const std::string* script_file)
{
	NetAccessDecision d;
	if(NetEnabled.isBool())
	{
		d.allowed = NetEnabled.asBool();
		d.why = std::string("\"NetEnabled\": ")+(d.allowed ? "true" : "false");
		return d;
	}
	if(NetEnabled.isNull() || (NetEnabled.isString() && NetEnabled.asString() == "FILE_SCRIPTS_ONLY"))
	{
		d.allowed = (script_file != nullptr);
		d.why = std::string("\"NetEnabled\": FILE_SCRIPTS_ONLY")+(NetEnabled.isNull() ? " (the default)" : "")+
		        (d.allowed ? ", and the script is from a file" : ", and the script is not from a file");
		return d;
	}
	if(NetEnabled.isObject())
	{
		for(const auto& key : NetEnabled.getMemberNames())
			if(key != "Allow")
				return Error("has an unknown member \""+key+"\" (it takes \"Allow\")");
		const auto& allow = NetEnabled["Allow"];
		if(!allow.isArray())
			return Error("\"Allow\" must be a list of files and directories");
		for(const auto& entry : allow)
			if(!entry.isString() || entry.asString().empty())
				return Error("\"Allow\" must be a list of files and directories");
		if(!script_file)
		{
			d.why = "\"NetEnabled\" has an allow list, and the script is not from a file";
			return d;
		}
		const auto script = Resolved(*script_file);
		for(const auto& entry : allow)
		{
			const auto allowed = Resolved(entry.asString());
			if(allowed.empty() || script.empty())
				continue;
			std::error_code ec;
			const bool match = fs::is_directory(allowed,ec) ? Under(script,allowed) : (script == allowed);
			if(match)
			{
				d.allowed = true;
				d.why = "\"NetEnabled\" allows \""+entry.asString()+"\"";
				return d;
			}
		}
		d.why = "\"NetEnabled\": the script is not in the allow list";
		return d;
	}
	return Error("must be true, false, \"FILE_SCRIPTS_ONLY\" or { \"Allow\": [...] }");
}
