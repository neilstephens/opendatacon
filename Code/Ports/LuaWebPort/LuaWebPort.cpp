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
 * LuaWebPort.cpp
 *
 *  Created on: 17/06/2023
 *      Author: Neil Stephens
 */

#include "LuaWebPort.h"
#include "LuaWebPortConf.h"
#include <Lua/CLua.h>
#include <Lua/Wrappers.h>
#include <opendatacon/util.h>

/* Test Certificate */
//openssl genrsa -out server.key 2048
const char default_cert_pem[] = R"001(
-----BEGIN CERTIFICATE-----
MIIFoTCCA4mgAwIBAgIUYNrdJQ/9gQy6DBV8O20G8Sxo0pcwDQYJKoZIhvcNAQEN
BQAwXzELMAkGA1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoM
GEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDEYMBYGA1UEAwwPb3BlbmRhdGFjb24u
bmV0MCAXDTIwMDkxNzIxMjkyM1oYDzIxMjAwODI0MjEyOTIzWjBfMQswCQYDVQQG
EwJBVTETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50ZXJuZXQgV2lk
Z2l0cyBQdHkgTHRkMRgwFgYDVQQDDA9vcGVuZGF0YWNvbi5uZXQwggIiMA0GCSqG
SIb3DQEBAQUAA4ICDwAwggIKAoICAQDOFlx0YR0kDhw5Ca1YplOTcmeRrUe3ELwt
3igMEP+0FfwgfxPXrCiJkN87mps8xR0qaUdI/99YFeu9vN9OQOkkSYbix7vqTFgJ
BmZD0ftYxR37npsojKqsFIu5vadiLgYR+r2Q3MeqIZJQ9kiMKi6chhqgX8LCEQnK
7rSDbBy1ed4RoHeu6EyjRrFB2ICPgZtBQtPkM+cI0SQ807Xu2Y2+VC4DCkWqc7Ku
G3QlsN5m0Sd0iN8ImFnxNKimw/gbFeWEp9cEuRsnGsBmCuN7bDzrxnofYYfKwNH+
YW0s8laAJg1Rie6sOh7NSZR3P0gabJm30rYC6jjuAvmrIFEGLVk0luQ7sj5Nr17q
eAUISvTtoC/tQqyXljSzbhW4EmAO5lTNsA7N10sCxDrrXiXPTN5MsyQLubs93/M0
So8f70/UMUPwMcXoEKwum1NEMnZ2vd19KZgUJ3DPsdN9RHORJxuzAqAZBdc5B9ua
R9g8xbl4aQkS7Zii5DaY3mYRJY/apKoBjLzOBjEcmoe4sm78+vUdLN7sH11DUYAL
idNTYD2tre6d+nAIaHf860kFEIIzxO4pSMedR+2MEhZ0kGJD8ZyJ/Jv9cazc4D7p
qn/VHAxUmDOHTn+ACbhBDfak1UakJbNt+yZtgSwaAnZwhY3BktKujTXGHNy1+0Iq
ZBFQ4sVJGQIDAQABo1MwUTAdBgNVHQ4EFgQUROGL8zWErJBNXCSMyojX0UkgXwsw
HwYDVR0jBBgwFoAUROGL8zWErJBNXCSMyojX0UkgXwswDwYDVR0TAQH/BAUwAwEB
/zANBgkqhkiG9w0BAQ0FAAOCAgEAteukOHqfnAJoPknpSIQLiL57kfvcKvyYoRXT
9aTi7tLOB6j0iZfu1MMlK2hmsRfCguwMKw7wUq3cYJD5esjKQvO6Us1c2zFxQdJM
cAyn2Ug3pUi2YPfLZsmdCKsxP4Im3m9T1SqSXrc6ibSIqtAWvnEvWF9fHErHvBln
lXumvsnAarJsgSCYKMDgoes2+az5iBi+7DQYGR0NZ24tUDMpGNbeptziirINBrZ2
KQ5FF6l0O90n1mykT4u2Z/1Xbv5v4gfmXt4HKrWaiL6NY9zM/GQEl36CIBNgVlz+
O68joZt+DPko9PLQ4aT3EOZD7XyA4OMcozuWhOe+pgnRghUus5yek8gyR0V0Jao7
nL8We1JM6J0W+hIBIawLJ5EeiDI6+7dct6V/UqecO+bjUP7Y8RPS0Y4h72Fb6mad
0EXvF+Ybis1xv7n8BvUA6+jUqw2Q9mWFVR6QWGSNy9MlyXw9/FlqOLByrQ7IEuqU
w25Wnzi3+OKZmiS5/mjWvZDoQH18NRO5FdgfHykPOlYMow1D/ojlZhrG+oYZKLgf
yCYfLUhaJ3Giqz3pvgZV3YatzER9lUihoSti5QtOqtoTJfzc3fJCiiBTeZjyvJ8r
bUrUAexSznSkuGKNRhhI9KAdKcF1ZurwV8Y9RqCvQzWY5bpxXKllkcAW84Pmws+M
MnvNKGA=
-----END CERTIFICATE-----
)001";

//openssl req -days 3650 -out server.pem -new -x509 -key server.key
const char default_key_pem[] = R"001(
-----BEGIN PRIVATE KEY-----
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
whkivOuTkh7GPlH1NEKqIKEA97Tw
-----END PRIVATE KEY-----
)001";

inline bool file_exists (const std::string& name)
{
	std::ifstream f(name.c_str());
	return f.good();
}

LuaWebPort::LuaWebPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
	DataPort(aName, aConfFilename, aConfOverrides)
{
	pConf = std::make_unique<LuaWebPortConf>();
	ProcessFile();

	// There are defaults for testing if the conf information is missing for the WebServer
	auto pConf = static_cast<LuaWebPortConf*>(this->pConf.get());
	const std::string& web_crt = pConf->web_crt;
	const std::string& web_key = pConf->web_key;
	WebSrv = make_shared<WebServer>(OPTIONAL_CERTS);
}

LuaWebPort::~LuaWebPort()
{
	// SJE: Need to ask Neil about this - test locks up if Disable is not called before the ptr is released
	if (enabled)
	{
		Disable_();
	}

	//Wait for outstanding handlers
	std::weak_ptr<void> tracker = handler_tracker;
	handler_tracker.reset();
	while(!tracker.expired() && !pIOS->stopped())
		pIOS->poll_one();

	lua_close(LuaState);
}

//only called on Lua sync strand
void LuaWebPort::Enable_()
{
	// Starts a thread for the webserver to process stuff on. See comment below.
	std::thread server_thread([this]()
		{
			// Start server
			WebSrv->start([](unsigned short port)
				{
					if (auto log = odc::spdlog_get("LuaWebPort"))
						log->info("Simple Web Server listening on port {}.", port);
				});
		});

	//TODO/FIXME: make thread a member, so we can join it on disable/shutdown
	//detaching not so bad for the moment
	server_thread.detach();

	CallLuaGlobalVoidVoidFunc("Enable");
}
//only called on Lua sync strand
void LuaWebPort::Disable_()
{
	WebSrv->stop();
	CallLuaGlobalVoidVoidFunc("Disable");
	//Force garbage collection now. Lingering shared_ptr finalizers can block shutdown etc
	lua_gc(LuaState,LUA_GCCOLLECT);
}

//Build is called while there's only one active thread, so we don't need to sync access to LuaState here
void LuaWebPort::Build()
{
	//top level table "odc"
	lua_newtable(LuaState);
	{
		//Export our name
		lua_pushstring(LuaState,Name.c_str());
		lua_setfield(LuaState,-2,"Name");

		//Export our JSON config
		PushJSON(LuaState,JSONConf);
		lua_setfield(LuaState,-2,"Config");
	}
	lua_setglobal(LuaState,"odc");

	ExportWrappersToLua(LuaState,pLuaSyncStrand,handler_tracker,Name,"LuaWebPort");
	ExportLuaPublishEvent();
	ExportLuaInDemand();
	luaL_openlibs(LuaState);

	//load the lua code
	auto pConf = static_cast<LuaWebPortConf*>(this->pConf.get());
	auto ret = luaL_dofile(LuaState, pConf->LuaFile.c_str());
	if(ret != LUA_OK)
	{
		std::string err = lua_tostring(LuaState, -1);
		throw std::runtime_error(Name+": Failed to load LuaFile '"+pConf->LuaFile+"'. Error: "+err);
	}

	for(const auto& func_name : {"Enable","Disable","Build","Event"})
	{
		lua_getglobal(LuaState, func_name);
		if(!lua_isfunction(LuaState, -1))
			throw std::runtime_error(Name+": Lua code doesn't have '"+func_name+"' function.");
	}

	WebSrv->config.port = pConf->port;

	//make a handler that simply posts the work and returns
	// we don't want the web server thread doing any actual work
	// because ODC needs full control via the main thread pool
	auto request_handler = [this](std::shared_ptr<WebServer::Response> response,
	                              std::shared_ptr<WebServer::Request> request)
				     {
					     pIOS->post([this, response, request]() {DefaultRequestHandler(response, request); });
				     };

	//TODO: we could use non-default resources to regex match the URL
	// then we could get rid of the URL parsing code in the handler
	// in favour of the regex match groups, and post the ultimate handlers directly
	WebSrv->default_resource["GET"] = request_handler;
	WebSrv->default_resource["POST"] = request_handler;

	// GET-example for the path /match/[number], responds with the matched string in path (number)
	// For instance a request GET /match/123 will receive: 123
	// For Test Code
	WebSrv->resource["^/match/([0-9]+)$"]["GET"] = [](std::shared_ptr<WebServer::Response> response, std::shared_ptr<WebServer::Request> request)
								     {
									     response->write(request->path_match[1].str());
								     };

	// Add resources using path-regex and method-string, and an anonymous function
	// POST-example for the path /string, responds the posted string
	// Testing the method that lua will call durign its build to add a handler
	RegisterRequestHandler("^/string$", "POST", "TestLuaHandler");

	CallLuaGlobalVoidVoidFunc("Build"); // In here the lua code will be able to add resources to the web server.
}

//only called on Lua sync strand
void LuaWebPort::Event_(std::shared_ptr<const EventInfo> event, const std::string& SenderName, SharedStatusCallback_t pStatusCallback)
{
	if(auto log = odc::spdlog_get("LuaWebPort"))
		log->trace("{}: {} event from {}", Name, ToString(event->GetEventType()), SenderName);

	//Get ready to call the lua function
	lua_getglobal(LuaState, "Event");

	//first arg
	PushEventInfo(LuaState,event);

	//put sendername on stack as second arg
	lua_pushstring(LuaState, SenderName.c_str());

	//put callback closure on stack as last/third arg
	//shared_ptr userdata up-value
	auto p = lua_newuserdatauv(LuaState,sizeof(std::shared_ptr<void>),0);
	new(p) std::shared_ptr<void>(pStatusCallback);
	luaL_getmetatable(LuaState, "std_shared_ptr_void");
	lua_setmetatable(LuaState, -2);
	lua_pushcclosure(LuaState, [](lua_State* const L) -> int
		{
			auto cb_void = static_cast<std::shared_ptr<void>*>(lua_touserdata(L, lua_upvalueindex(1)));
			auto cb = std::static_pointer_cast<SharedStatusCallback_t::element_type>(*cb_void);
			auto cmd_stat = static_cast<CommandStatus>(lua_tointeger(L, -1));
			(*cb)(cmd_stat);
			return 0; //number of lua ret vals pushed onto the stack
		}, 1);

	//now call lua Event()
	const int argc = 3; const int retc = 0;
	auto ret = lua_pcall(LuaState,argc,retc,0);
	if(ret != LUA_OK)
	{
		std::string err = lua_tostring(LuaState, -1);
		if(auto log = odc::spdlog_get("LuaWebPort"))
			log->error("{}: Lua Event() call error: {}",Name,err);
		lua_pop(LuaState,1);
	}
}

void LuaWebPort::ProcessElements(const Json::Value& JSONRoot)
{
	if(!JSONRoot.isObject()) return;

	auto MemberNames = JSONRoot.getMemberNames();
	for(auto mn : MemberNames)
		JSONConf[mn] = JSONRoot[mn];

	auto pConf = static_cast<LuaWebPortConf*>(this->pConf.get());

	if(JSONRoot.isMember("LuaFile"))
		pConf->LuaFile = JSONRoot["LuaFile"].asString();

	if(JSONRoot.isMember("IP"))
		pConf->ip= JSONRoot["IP"].asString();
	if(JSONRoot.isMember("Port"))
		pConf->port = JSONRoot["Port"].asUInt();
	if (JSONRoot.isMember("WebCert"))
		pConf->web_crt = JSONRoot["WebCert"].asString();
	if (JSONRoot.isMember("WebKey"))
		pConf->web_key = JSONRoot["WebKey"].asString();

	// If the certificates dont exist write them out so we have a default set - just for testing??
	if(!file_exists(pConf->web_crt))
	{
		std::ofstream fout(pConf->web_crt);
		fout<<default_cert_pem;
		fout.flush();
	}
	if(!file_exists(pConf->web_key))
	{
		std::ofstream fout(pConf->web_key);
		fout<<default_key_pem;
		fout.flush();
	}
}

typedef std::shared_ptr<std::function<void (std::shared_ptr<WebServer::Response> response, std::shared_ptr<WebServer::Request> request)>> SharedRequestHandler;

// So pass to this method the required parameters and a callback - how do we make the callback a lua function?
void LuaWebPort::RegisterRequestHandler(const std::string urlpattern, const std::string method, const std::string LuaHandlerName)
{
	if (auto log = odc::spdlog_get("LuaWebPort"))
		log->debug("Registering a lua web handler {} {} Lua Method {}", urlpattern, method, LuaHandlerName);

	WebSrv->resource[urlpattern][method] = [this, urlpattern, method, LuaHandlerName](std::shared_ptr<WebServer::Response> response, std::shared_ptr<WebServer::Request> request)
							   {
								   // So set up everything necessary to call the lua method now. We cant type check it, just have to hope we are good.
								   // We have to work out how to translate the request (could just be a string) and the response - has methods attached into a lua compatible method
								   // The response needs to have stuff set.

								   if (auto log = odc::spdlog_get("LuaWebPort"))
									   log->debug("Calling a lua web handler {} {} Lua Method {}", urlpattern, method, LuaHandlerName);

								   //Get ready to call the lua function
								   lua_getglobal(LuaState, LuaHandlerName.c_str());

								   auto path = request->path;
								   auto query = request->query_string;
								   auto content = request->content.string();
								   auto contentlength = content.length();
								   auto pathmatch = request->path_match;
								   //first 3 string arguments
								   // TestLuaHandler(method, path, query, contentlength, content)
								   lua_pushstring(LuaState, method.c_str());
								   lua_pushstring(LuaState, path.c_str());
								   lua_pushstring(LuaState, query.c_str());
								   lua_pushinteger(LuaState, contentlength);
								   if (contentlength > 0)
									   lua_pushlstring(LuaState, content.c_str(), contentlength); // Neil has a warning about 0 lentgh strings and this method
								   else
									   lua_pushstring(LuaState, content.c_str());

								   //now call lua Event()
								   const int argc = 5; const int retc = 0;
								   auto ret = lua_pcall(LuaState, argc, retc, 0);
								   if (ret != LUA_OK)
								   {
									   std::string err = lua_tostring(LuaState, -1);
									   if (auto log = odc::spdlog_get("LuaWebPort"))
										   log->error("{}: Lua Event() call error: {}", Name, err);
									   lua_pop(LuaState, 1);
								   }

								   auto responsecontent = content;
								   int ErrorCode = 0;
								   auto responsecontenttype = "application/json";

								   // So if we are just sending text with no need to specifiy type, just use the write(content)
								   // However the type should probably always be specified, so make that the standard. default is application/octet-stream
								   // We could also return an errorcode and an associated message
								   // So the retun values from the lua code will be:
								   // ErrorCode, ContentType, ContentLength, Content	// ErrorCode is in status_code.hpp from the SimpleWeb code - line 77 If it is 0, not error respond normally.
								   // { "application/json", "application/xml", "text/html", "text/plain", "application/octet-stream" }
								   if (ErrorCode == 0)
								   {
									   SimpleWeb::CaseInsensitiveMultimap header;
									   header.emplace("Content-Length", std::to_string(responsecontent.length()));
									   header.emplace("Content-Type", responsecontenttype);
									   response->write(header);

									   response->write(responsecontent); // Could be a http response - serving a web page, or xml or other.
								   }
								   else
									   response->write(static_cast<SimpleWeb::StatusCode>(ErrorCode), responsecontent); // Write an error response...
							   };
}

//This is only called from Build(), so no sync required.
void LuaWebPort::ExportLuaPublishEvent()
{
	lua_getglobal(LuaState,"odc");
	//push closure with one upvalue (to capture 'this')
	lua_pushlightuserdata(LuaState, this);
	lua_pushcclosure(LuaState, [](lua_State* const L) -> int
		{
			//retrieve 'this'
			auto self = static_cast<const LuaWebPort*>(lua_topointer(L, lua_upvalueindex(1)));

			//first arg is EventInfo
			auto event = EventInfoFromLua(L,self->Name,"LuaWebPort",1);
			if(!event)
			{
				lua_pushboolean(L, false);
				return 1;
			}
			if(event->GetSourcePort()=="")
				event->SetSource(self->Name);

			//optional second arg is command status callback
			if(lua_isfunction(L,2))
			{
				lua_pushvalue(L,2);                             //push a copy, because luaL_ref pops
				auto LuaCBref = luaL_ref(L, LUA_REGISTRYINDEX); //pop
				auto cb = std::make_shared<std::function<void (CommandStatus status)>>(self->pLuaSyncStrand->wrap([L,LuaCBref,self,h{self->handler_tracker}](CommandStatus status)
					{
						auto lua_status = static_cast< std::underlying_type_t<odc::CommandStatus> >(status);
						//get callback from the registry back on stack
						lua_rawgeti(L, LUA_REGISTRYINDEX, LuaCBref);
						//push the status argument
						lua_pushinteger(L,lua_status);

						//now call
						const int argc = 1; const int retc = 0;
						auto ret = lua_pcall(L,argc,retc,0);
						if(ret != LUA_OK)
						{
							std::string err = lua_tostring(L, -1);
							if(auto log = odc::spdlog_get("LuaWebPort"))
								log->error("{}: Lua PublishEvent() callback error: {}",self->Name,err);
							lua_pop(L,1);
						}
						//release the reference to the callback
						luaL_unref(L, LUA_REGISTRYINDEX, LuaCBref);
					}));
				self->PublishEvent(event, cb);
			}
			else
				self->PublishEvent(event);

			lua_pushboolean(L, true);
			return 1; //number of lua ret vals pushed onto the stack
		}, 1);
	lua_setfield(LuaState,-2,"PublishEvent");
}

//This is only called from Build(), so no sync required.
void LuaWebPort::ExportLuaInDemand()
{
	lua_getglobal(LuaState,"odc");
	//push closure with one upvalue (to capture 'this')
	lua_pushlightuserdata(LuaState, this);
	lua_pushcclosure(LuaState, [](lua_State* const L) -> int
		{
			//retrieve 'this'
			auto self = static_cast<const LuaWebPort*>(lua_topointer(L, lua_upvalueindex(1)));
			auto in_demand = self->InDemand();
			lua_pushboolean(L, in_demand);
			return 1;
		}, 1);
	lua_setfield(LuaState,-2,"InDemand");
}

//Must only be called from the the LuaState sync strand
void LuaWebPort::CallLuaGlobalVoidVoidFunc(const std::string& FnName)
{
	lua_getglobal(LuaState, FnName.c_str());

	//no args, so just call it
	const int argc = 0; const int retc = 0;
	auto ret = lua_pcall(LuaState,argc,retc,0);
	if(ret != LUA_OK)
	{
		std::string err = lua_tostring(LuaState, -1);
		if(auto log = odc::spdlog_get("LuaWebPort"))
			log->error("{}: Lua {}() call error: {}",Name,FnName,err);
		lua_pop(LuaState,1);
	}
}
// Can SimpleWebServer do this for us?
void LuaWebPort::LoadRequestParams(std::shared_ptr<WebServer::Request> request)
{
	params.clear();
	if (request->method == "POST" && request->content.size() > 0)
	{
		auto type_pair_it = request->header.find("Content-Type");
		if (type_pair_it != request->header.end())
		{
			if (type_pair_it->second.find("application/x-www-form-urlencoded") != type_pair_it->second.npos)
			{
				auto content = SimpleWeb::QueryString::parse(request->content.string());
				for (auto& content_pair : content)
					params[content_pair.first] = content_pair.second;
			}
			else if (type_pair_it->second.find("application/json") != type_pair_it->second.npos)
			{
				Json::CharReaderBuilder JSONReader;
				std::string err_str;
				Json::Value Payload;
				bool parse_success = Json::parseFromStream(JSONReader, request->content, &Payload, &err_str);
				if (parse_success)
				{
					try
					{
						if (Payload.isObject())
						{
							for (auto& membername : Payload.getMemberNames())
								params[membername] = Payload[membername].asString();
						}
						else
							throw std::runtime_error("Payload isn't object");
					}
					catch (std::exception& e)
					{
						if (auto log = odc::spdlog_get("WebUI"))
							log->error("JSON POST paylod not suitable: {} : '{}'", e.what(), Payload.toStyledString());
					}
				}
				else if (auto log = odc::spdlog_get("WebUI"))
					log->error("Failed to parse POST payload as JSON: {}", err_str);
			}
			else if (auto log = odc::spdlog_get("WebUI"))
				log->error("unsupported POST 'Content-Type' : '{}'", type_pair_it->second);
		}
		else if (auto log = odc::spdlog_get("WebUI"))
			log->error("POST has no 'Content-Type'");
	}
}

void LuaWebPort::DefaultRequestHandler(std::shared_ptr<WebServer::Response> response,
	std::shared_ptr<WebServer::Request> request)
{
	LoadRequestParams(request);

	auto raw_path = SimpleWeb::Percent::decode(request->path);
	if (IsCommand(raw_path))
	{
		HandleCommand(raw_path, [response](const Json::Value&& json_resp)
			{
				SimpleWeb::CaseInsensitiveMultimap header;
				header.emplace("Content-Type", "application/json");

				Json::StreamWriterBuilder wbuilder;
				wbuilder["commentStyle"] = "None";
				auto str_resp = Json::writeString(wbuilder, json_resp);

				response->write(str_resp, header);
			});
	}
}

void LuaWebPort::HandleCommand(const std::string& url, std::function<void(const Json::Value&&)> result_cb)
{
	std::stringstream iss;
	std::string responder;
	std::string command;
	ParseURL(url, responder, command, iss);

	//if (Responders.find(responder) != Responders.end())
	//{
	//	//ExecuteCommand(Responders[responder], command, iss, result_cb);
	//	pass;
	//}
	//else
	{
		Json::Value value;
		value["Result"] = "Responder not available.";
		result_cb(std::move(value));
	}
}

void LuaWebPort::ParseURL(const std::string& url, std::string& responder, std::string& command, std::stringstream& ss)
{
	std::stringstream iss(url);
	iss.get(); //throw away leading '/'
	iss >> responder;
	iss >> command;
	std::string params;
	std::string w;
	while (iss >> w)
		params += w + " ";
	if (!params.empty() && params[params.size() - 1] == ' ')
	{
		params = params.substr(0, params.size() - 1);
	}
	std::stringstream ss_temp(params);
	ss << ss_temp.rdbuf();
}

bool LuaWebPort::IsCommand(const std::string& url)
{
	std::stringstream iss(url);
	std::string responder;
	std::string command;
	iss.get(); //throw away leading '/'
	iss >> responder >> command;
	bool result = false;
	//if ((Responders.find(responder) != Responders.end()) ||
	//	(RootCommands.find(command) != RootCommands.end()) ||
	//	responder == "WebUICommand")
	//{
	//	result = true;
	//}
	return result;
}