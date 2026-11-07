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
 * NetAccessTest.cpp
 *
 *      Author: Neil Stephens <dearknarl@gmail.com>
 *
 *  The "NetEnabled" decision (NetAccess.h): which Lua scripts get the network objects.
 */
#include <NetAccess.h>
#include <catch.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#define SUITE(name) "NetAccess - " name

namespace
{
namespace fs = std::filesystem;

Json::Value Parse(const std::string& text)
{
	Json::Value v;
	Json::CharReaderBuilder b;
	std::string err;
	std::istringstream in(text);
	REQUIRE(Json::parseFromStream(b,in,&v,&err));
	return v;
}

struct Dir
{
	fs::path root;
	Dir()
	{
		static std::atomic<unsigned> n{0};
		root = fs::temp_directory_path()/("odc_netaccess_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"_"+std::to_string(n++));
		fs::create_directories(root);
	}
	~Dir()
	{
		std::error_code ec;
		fs::remove_all(root,ec);
	}
	std::string File(const std::string& rel)
	{
		auto p = root/rel;
		fs::create_directories(p.parent_path());
		std::ofstream(p) << "-- a script\n";
		return p.string();
	}
};
}

TEST_CASE(SUITE("true, false and FILE_SCRIPTS_ONLY (the default)"))
{
	const std::string file = "some_script.lua";
	CHECK(DecideNetAccess(Json::Value(true),&file).allowed);
	CHECK(DecideNetAccess(Json::Value(true),nullptr).allowed);
	CHECK_FALSE(DecideNetAccess(Json::Value(false),&file).allowed);
	CHECK_FALSE(DecideNetAccess(Json::Value(false),nullptr).allowed);
	for(const auto& v : {Json::Value(),Json::Value("FILE_SCRIPTS_ONLY")})
	{
		CHECK(DecideNetAccess(v,&file).allowed);
		CHECK_FALSE(DecideNetAccess(v,nullptr).allowed);
		CHECK_FALSE(DecideNetAccess(v,&file).config_error);
	}
	CHECK(DecideNetAccess(Json::Value(),&file).why.find("default") != std::string::npos);
}

TEST_CASE(SUITE("anything else fails closed, as a configuration error"))
{
	const std::string file = "some_script.lua";
	for(const auto& text : {R"("yes")",R"("file_scripts_only")","1","0",R"([])",R"({"Allow": "a/b"})",R"({"Allow": [1]})",R"({"Allow": [""]})",
	                        R"({"Allow": ["a"], "Deny": ["b"]})",R"({})"})
	{
		INFO(text);
		const auto d = DecideNetAccess(Parse(text),&file);
		CHECK_FALSE(d.allowed);
		CHECK(d.config_error);
		CHECK(d.why.find("\"NetEnabled\"") != std::string::npos);
	}
}

TEST_CASE(SUITE("Allow: directories and files, after resolving links and .."))
{
	Dir d;
	const auto a = d.File("A/a.lua");
	const auto a_sub = d.File("A/sub/b.lua");
	const auto b = d.File("B/c.lua");
	const auto sibling = d.File("A_x/d.lua");
	const auto other = d.File("B/other.lua");
	const auto root = d.root.string();
	Json::Value conf;
	conf["Allow"].append(root+"/A");
	conf["Allow"].append(root+"/B/c.lua");
	auto allowed = [&](const std::string& f)
			   {
				   const auto r = DecideNetAccess(conf,&f);
				   CHECK_FALSE(r.config_error);
				   return r.allowed;
			   };
	CHECK(allowed(a));
	CHECK(allowed(a_sub));
	CHECK(allowed(b));
	CHECK(allowed(root+"/A/../B/c.lua"));               //the same file, by another path
	CHECK_FALSE(allowed(sibling));                      //A_x is not under A
	CHECK_FALSE(allowed(other));                        //only c.lua of B
	CHECK_FALSE(allowed(root+"/A/../B/other.lua"));     //out of A by ".."
	CHECK_FALSE(DecideNetAccess(conf,nullptr).allowed); //code not from a file

	//a link inside an allowed directory to a file outside it is the file outside it
	std::error_code ec;
	fs::create_symlink(other,d.root/"A"/"link.lua",ec);
	if(!ec)
		CHECK_FALSE(allowed(root+"/A/link.lua"));
	else
		WARN("symbolic links can't be made here: "+ec.message());

	//an allowed directory given with a trailing separator, and a relative path (to the working directory)
	Json::Value slash;
	slash["Allow"].append(root+"/A/");
	CHECK(DecideNetAccess(slash,&a).allowed);
	Json::Value rel;
	rel["Allow"].append(fs::relative(d.root/"A",fs::current_path()).string());
	CHECK(DecideNetAccess(rel,&a).allowed);
	const auto rel_script = fs::relative(a,fs::current_path()).string();
	CHECK(DecideNetAccess(rel,&rel_script).allowed);

	//an empty list allows nothing
	Json::Value none;
	none["Allow"] = Json::Value(Json::arrayValue);
	CHECK_FALSE(DecideNetAccess(none,&a).allowed);
	CHECK_FALSE(DecideNetAccess(none,&a).config_error);
}
