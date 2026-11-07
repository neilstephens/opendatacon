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
 * NetWrappers.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  odc.TCPSocket and odc.TLSSocket: a TCPSessionManager (plain or TLS) for Lua scripts. One userdata type and one object class (SockObj); what
 *  differs is the construction and a few lines in the event handlers.
 *
 *  Threading: the manager's callbacks run on its own strand. They only copy what they are given and hop to the Lua strand (holding the
 *  port's handler_tracker, as msTimerCallback does). Everything else here, including every touch of the lua_State, runs on the Lua strand.
 *
 *  Lifetime: the Lua handle (a userdata) owns a SockObj through a shared_ptr. While the object is open it is also in the context's 'live' set
 *  and the handle is held by a registry reference, so neither is collected until the object ends. A SockObj is never destroyed on the Lua strand
 *  or inside one of its own callbacks: the shared_ptr's deleter posts the destruction to the io service (the manager's destructor waits for handlers).
 *
 *  TLS needs no compile time switch here: the TLS classes are in every build of the library, and without OpenSSL tls::TlsSupported() is false
 *  and odc.TLSSocket returns nil, "no_tls".
 */
#include "NetWrappers.h"
#include "HttpWrappers.h"
#include "NetCommon.h"
#include <Lua/CLua.h>
#include <json/json.h>
#include <opendatacon/TCPSessionManager.h>
#include <opendatacon/spdlog.h>
#include <opendatacon/util.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

namespace
{
const char* const SockMeta = "odc.Socket";
const char* const CtxMeta = "odc.NetContext";
const char* const CtxKey = "odc.NetContext";

constexpr uint64_t MaxTimeMs = std::numeric_limits<int32_t>::max();

std::atomic<size_t> LiveObjects{0};

using odcnet::PortInfo;
using odcnet::LogMsg;

struct SockOpts
{
	bool tls = false;
	std::string host;
	std::string port;
	bool server = false;
	bool auto_reopen = false;
	bool keepalive = true;
	uint16_t retry_ms = 0;
	uint64_t connect_timeout_ms = 10000;
	uint64_t idle_timeout_ms = 0;
	size_t write_buffer_limit = 0; //0 is no limit on the number of queued writes (TCP only)
	uint64_t throttle_bitrate = 0;
	uint64_t throttle_chunksize = 0;
	uint64_t throttle_writedelay_ms = 0;
	unsigned int keepalive_idle_s = 599;
	unsigned int keepalive_retry_s = 10;
	unsigned int keepalive_count = 3;
	//TLS only
	std::string peer_name;
	odc::tls::TlsConf tls_conf;
	std::shared_ptr<odc::tls::Context> tls_context;
};

bool IsLoopback(const std::string& host)
{
	return host == "localhost" || host == "::1" || host.rfind("127.",0) == 0;
}

//Reads the options table at idx. Returns "" or the name of the first option that is not valid.
class OptReader
{
public:
	OptReader(lua_State* L, int idx): L(L), idx(lua_absindex(L,idx)) {}

	bool Bool(const char* name, bool& out)
	{
		lua_getfield(L,idx,name);
		const int t = lua_type(L,-1);
		bool ok = (t == LUA_TNIL || t == LUA_TBOOLEAN);
		if(t == LUA_TBOOLEAN)
			out = lua_toboolean(L,-1);
		lua_pop(L,1);
		return ok;
	}
	//an integer (or a float with an integer value) from 0 to max
	template<typename T>
	bool UInt(const char* name, T& out, uint64_t max)
	{
		lua_getfield(L,idx,name);
		const int t = lua_type(L,-1);
		bool ok = (t == LUA_TNIL);
		if(t == LUA_TNUMBER)
		{
			int isnum = 0;
			auto v = lua_tointegerx(L,-1,&isnum);
			ok = isnum && v >= 0 && static_cast<uint64_t>(v) <= max;
			if(ok)
				out = static_cast<T>(v);
		}
		lua_pop(L,1);
		return ok;
	}
	bool Str(const char* name, std::string& out, bool& present)
	{
		lua_getfield(L,idx,name);
		const int t = lua_type(L,-1);
		present = (t != LUA_TNIL);
		bool ok = (t == LUA_TNIL);
		if(t == LUA_TSTRING)
		{
			size_t len = 0;
			auto s = lua_tolstring(L,-1,&len);
			ok = len > 0 && len <= 255 && strlen(s) == len;
			if(ok)
				out.assign(s,len);
		}
		lua_pop(L,1);
		return ok;
	}
	//a port number: an integer, or a string of digits, from 1 to 65535
	bool Port(const char* name, std::string& out, bool& present)
	{
		lua_getfield(L,idx,name);
		const int t = lua_type(L,-1);
		present = (t != LUA_TNIL);
		bool ok = (t == LUA_TNIL);
		if(t == LUA_TNUMBER || t == LUA_TSTRING)
		{
			uint64_t v = 0;
			if(t == LUA_TNUMBER)
			{
				int isnum = 0;
				auto i = lua_tointegerx(L,-1,&isnum);
				ok = isnum && i >= 1 && i <= 65535;
				v = static_cast<uint64_t>(i);
			}
			else
			{
				std::string s(lua_tostring(L,-1));
				ok = !s.empty() && s.size() <= 5 && s.find_first_not_of("0123456789") == std::string::npos;
				if(ok)
					v = std::stoull(s);
				ok = ok && v >= 1 && v <= 65535;
			}
			if(ok)
				out = std::to_string(v);
		}
		lua_pop(L,1);
		return ok;
	}
	bool Func(const char* name)
	{
		lua_getfield(L,idx,name);
		const int t = lua_type(L,-1);
		lua_pop(L,1);
		return t == LUA_TNIL || t == LUA_TFUNCTION;
	}
private:
	lua_State* L;
	int idx;
};

//Returns "" if the options are valid, else the name of the first invalid one
std::string ParseSockOpts(lua_State* L, int idx, SockOpts& o)
{
	static const std::set<std::string> known
	{
		"host","port","server","auto_reopen","retry_ms","connect_timeout_ms","idle_timeout_ms","write_buffer_limit",
		"throttle_bitrate","throttle_chunksize","throttle_writedelay_ms","keepalive","keepalive_idle_s",
		"keepalive_retry_s","keepalive_count","on_open","on_data","on_close","on_error"
	};
	static const std::set<std::string> known_tls{"peer_name","tls","on_tls_failed"};
	idx = lua_absindex(L,idx);

	//typos must not be silently ignored
	lua_pushnil(L);
	while(lua_next(L,idx))
	{
		std::string key = (lua_type(L,-2) == LUA_TSTRING) ? lua_tostring(L,-2) : "(non-string key)";
		lua_pop(L,1); //the value, leaving the key for lua_next
		if(!known.count(key) && !(o.tls && known_tls.count(key)))
		{
			lua_pop(L,1);
			return key;
		}
	}

	OptReader r(L,idx);
	bool host_present = false;
	bool port_present = false;
	if(!r.Bool("server",o.server)) return "server";
	if(!r.Str("host",o.host,host_present)) return "host";
	if(!host_present)
	{
		if(!o.server)
			return "host";
		o.host = "127.0.0.1";
	}
	if(!r.Port("port",o.port,port_present) || !port_present) return "port";
	if(!r.Bool("auto_reopen",o.auto_reopen)) return "auto_reopen";
	if(!r.UInt("retry_ms",o.retry_ms,std::numeric_limits<uint16_t>::max())) return "retry_ms";
	if(!r.UInt("connect_timeout_ms",o.connect_timeout_ms,MaxTimeMs)) return "connect_timeout_ms";
	if(!r.UInt("idle_timeout_ms",o.idle_timeout_ms,MaxTimeMs)) return "idle_timeout_ms";
	if(!r.UInt("write_buffer_limit",o.write_buffer_limit,std::numeric_limits<uint32_t>::max())) return "write_buffer_limit";
	if(!r.UInt("throttle_bitrate",o.throttle_bitrate,std::numeric_limits<uint32_t>::max())) return "throttle_bitrate";
	if(!r.UInt("throttle_chunksize",o.throttle_chunksize,std::numeric_limits<uint32_t>::max())) return "throttle_chunksize";
	if(!r.UInt("throttle_writedelay_ms",o.throttle_writedelay_ms,MaxTimeMs)) return "throttle_writedelay_ms";
	if(!r.Bool("keepalive",o.keepalive)) return "keepalive";
	if(!r.UInt("keepalive_idle_s",o.keepalive_idle_s,604800)) return "keepalive_idle_s";
	if(!r.UInt("keepalive_retry_s",o.keepalive_retry_s,86400)) return "keepalive_retry_s";
	if(!r.UInt("keepalive_count",o.keepalive_count,255)) return "keepalive_count";
	for(const char* cb : {"on_open","on_data","on_close","on_error"})
		if(!r.Func(cb))
			return cb;
	if(o.tls)
	{
		bool peer_present = false;
		if(!r.Str("peer_name",o.peer_name,peer_present)) return "peer_name";
		if(!r.Func("on_tls_failed")) return "on_tls_failed";
	}
	if(o.write_buffer_limit == 0)
		o.write_buffer_limit = std::numeric_limits<size_t>::max();
	return "";
}

//A Lua value as JSON, for tls::ParseTlsConf. False, with what is wrong in why, if there is no JSON form (a function, a table of mixed keys ...)
bool LuaToJson(lua_State* L, int idx, Json::Value& out, const std::string& path, std::string& why, int depth = 0)
{
	idx = lua_absindex(L,idx);
	switch(lua_type(L,idx))
	{
		case LUA_TBOOLEAN:
			out = static_cast<bool>(lua_toboolean(L,idx));
			return true;
		case LUA_TNUMBER:
		{
			int isint = 0;
			auto i = lua_tointegerx(L,idx,&isint);
			if(isint)
				out = static_cast<Json::Int64>(i);
			else
				out = lua_tonumber(L,idx);
			return true;
		}
		case LUA_TSTRING:
		{
			size_t len = 0;
			auto s = lua_tolstring(L,idx,&len);
			out = std::string(s,len);
			return true;
		}
		case LUA_TTABLE:
		{
			if(depth > 3)
			{
				why = path+" is nested too deeply";
				return false;
			}
			Json::Value obj(Json::objectValue);
			std::map<lua_Integer,Json::Value> arr;
			bool all_str = true;
			bool all_int = true;
			bool ok = true;
			size_t count = 0;
			lua_pushnil(L);
			while(ok && lua_next(L,idx))
			{
				++count;
				Json::Value v;
				if(lua_type(L,-2) == LUA_TSTRING)
				{
					all_int = false;
					const std::string k = lua_tostring(L,-2);
					ok = LuaToJson(L,-1,v,k,why,depth+1);
					obj[k] = v;
				}
				else if(lua_isinteger(L,-2))
				{
					all_str = false;
					ok = LuaToJson(L,-1,v,path+"[]",why,depth+1);
					arr[lua_tointeger(L,-2)] = v;
				}
				else
				{
					why = path+" has a key that is neither a string nor an integer";
					ok = false;
				}
				if(ok)
					lua_pop(L,1);
			}
			if(!ok)
			{
				lua_pop(L,2); //the key and the value lua_next left
				return false;
			}
			if(count == 0)
			{
				out = Json::Value(Json::arrayValue);
				return true;
			}
			if(all_str)
			{
				out = obj;
				return true;
			}
			lua_Integer expect = 1;
			if(all_int)
			{
				Json::Value array(Json::arrayValue);
				for(const auto& e : arr)
				{
					if(e.first != expect++)
					{
						why = path+" is a list with gaps";
						return false;
					}
					array.append(e.second);
				}
				out = array;
				return true;
			}
			why = path+" mixes names and positions";
			return false;
		}
		default:
			why = path+" has a value that is not a boolean, number, string or table";
			return false;
	}
}

std::string LowerCase(std::string s)
{
	std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){ return static_cast<char>(std::tolower(c)); });
	return s;
}

//The "tls" option: the TLS section of a port configuration, used as the library parses it. Returns "" or what to return to the script as the error.
std::string ParseTlsOption(lua_State* L, int idx, SockOpts& o)
{
	idx = lua_absindex(L,idx);
	lua_getfield(L,idx,"tls");
	if(!lua_istable(L,-1))
	{
		lua_pop(L,1);
		return "invalid_options: tls";
	}
	//(a script cannot ask for the TLS secrets to be written to a file)
	const int tls_idx = lua_gettop(L);
	lua_pushnil(L);
	while(lua_next(L,tls_idx))
	{
		const bool log_key = lua_type(L,-2) == LUA_TSTRING && LowerCase(lua_tostring(L,-2)) == "keylogfile";
		lua_pop(L,1);
		if(log_key)
		{
			lua_pop(L,2);
			return "invalid_options: tls.KeyLogFile";
		}
	}
	Json::Value section;
	std::string why;
	const bool converted = LuaToJson(L,tls_idx,section,"tls",why);
	lua_pop(L,1);
	if(!converted)
		return "tls config: "+why;
	if(!section.isObject())
		return "invalid_options: tls";

	//the parser ignores the section unless it is enabled; and the binding verifies the server's name unless told not to
	section["Enabled"] = true;
	if(!o.server && !section.isMember("VerifyHostname"))
		section["VerifyHostname"] = true;
	Json::Value root(Json::objectValue);
	root["TLS"] = section;
	std::vector<std::string> problems;
	odc::tls::ParseTlsConf(root,o.server,o.tls_conf,problems);
	if(!problems.empty())
	{
		std::string all;
		for(const auto& p : problems)
			all += (all.empty() ? "" : "; ")+p;
		return "tls config: "+all;
	}
	std::string error;
	o.tls_context = odc::tls::Context::Create(o.tls_conf,o.server ? odc::tls::Role::Server : odc::tls::Role::Client,error);
	if(!o.tls_context)
		return error.empty() ? "tls config: the TLS context could not be created" : error;
	return "";
}

//what is needed from the context by the objects, and by ExportNetWrappers/CloseNetObjects
class SockObj;
struct NetCtx
{
	PortInfo port;
	std::set<std::shared_ptr<SockObj>> live; //Lua strand only
	bool dead = false;                       //the port is being destroyed
};

enum class State { Created, Opened, Closing, Closed };

class SockObj: public std::enable_shared_from_this<SockObj>
{
public:
	//The deleter posts the destruction to the io service, so it never happens on the Lua strand or inside a callback of the object
	static std::shared_ptr<SockObj> Create(const PortInfo& port, std::weak_ptr<NetCtx> ctx, const SockOpts& opts)
	{
		auto ios = odc::asio_service::Get();
		std::shared_ptr<SockObj> p(new SockObj(port,std::move(ctx),opts), [ios](SockObj* o)
			{
				if(ios->stopped())
					delete o;
				else
					ios->post([up{std::unique_ptr<SockObj>(o)}](){});
			});
		p->Init();
		return p;
	}

	~SockObj()
	{
		//an object that was not closed (its script did not, and the port has gone) must not leave its manager running: its destructor would wait for handlers that are re-armed forever
		if(state == State::Opened || state == State::Closing)
			pMgr->Close();
		//the manager's destructor waits for its handlers, which use nothing of ours: so go first
		pMgr.reset();
		LogMsg(port.logname,spdlog::level::debug,port.name,kind+" "+desc+" destroyed.");
		--LiveObjects;
	}

	//---- the Lua methods (Lua strand)
	int LuaOpen(lua_State* L)
	{
		if(state == State::Closing || state == State::Closed)
			return PushError(L,"closed");
		if(state == State::Opened)
			return PushTrue(L);
		auto ctx = ctx_weak.lock();
		if(!ctx || ctx->dead)
			return PushError(L,"aborted");
		state = State::Opened;
		ctx->live.insert(shared_from_this());
		lua_pushvalue(L,1);
		handle_ref = luaL_ref(L,LUA_REGISTRYINDEX);
		LogMsg(port.logname,spdlog::level::debug,port.name,kind+" "+desc+" open.");
		ArmConnectTimer();
		pMgr->Open();
		return PushTrue(L);
	}

	int LuaWrite(lua_State* L, bool to_serial)
	{
		const int arg = to_serial ? 3 : 2;
		uint64_t serial = 0;
		if(to_serial)
		{
			int isnum = 0;
			auto s = lua_tointegerx(L,2,&isnum);
			if(!isnum || s < 0)
				return PushError(L,"invalid_options: serial");
		}
		size_t len = 0;
		auto data = luaL_checklstring(L,arg,&len);
		if(to_serial)
			serial = static_cast<uint64_t>(lua_tointeger(L,2));
		if(state != State::Opened)
			return PushError(L,"closed");
		//(a TLS connection takes writes from when the TCP connection is up: they wait for the handshake. Once its session has ended they are refused)
		if(!connected || (to_serial && serial != current_serial) || session_ended)
			return PushError(L,"closed");
		if(len == 0)
			return PushTrue(L);
		std::string buf(data,len);
		if(to_serial)
			pMgr->Write(std::move(buf),serial);
		else
			pMgr->Write(std::move(buf));
		last_activity = std::chrono::steady_clock::now();
		return PushTrue(L);
	}

	int LuaClose(lua_State* L)
	{
		if(state == State::Created || (state == State::Opened && !connected))
			Terminate("closed","",false);
		else if(state == State::Opened)
		{
			//a connection is up: end it (the manager sends what was written first) and stop reconnecting, then tell the script (after this call has returned)
			state = State::Closing;
			StopTimers();
			Hop([](SockObj& o){ o.Terminate("closed","",true); });
		}
		return PushTrue(L);
	}

	int LuaOn(lua_State* L)
	{
		std::string ev = luaL_checkstring(L,2);
		if(ev.rfind("on_",0) == 0)
			ev = ev.substr(3);
		const bool known = ev == "open" || ev == "data" || ev == "close" || ev == "error" || (opts.tls && ev == "tls_failed");
		if(!known)
			return PushError(L,"invalid_options: "+ev);
		if(!lua_isfunction(L,3) && !lua_isnil(L,3))
			return PushError(L,"invalid_options: "+ev);
		if(state == State::Closing || state == State::Closed)
			return PushError(L,"closed");
		lua_getiuservalue(L,1,1);
		lua_pushvalue(L,3);
		lua_setfield(L,-2,ev.c_str());
		lua_pop(L,1);
		return PushTrue(L);
	}

	//the last peer certificate and session of a TLS object, or nil
	int LuaPeer(lua_State* L)
	{
		if(!last_peer)
		{
			lua_pushnil(L);
			return 1;
		}
		PushPeer(L,*last_peer);
		return 1;
	}

	//TCP: a connection is up. TLS: the session is established
	bool IsOpen() const
	{
		return state == State::Opened && (opts.tls ? established : connected);
	}
	//the number of the TCP connection that is up (for TLS from when the TCP connection is up, before the handshake is done)
	uint64_t Serial() const
	{
		return (state == State::Opened && connected) ? current_serial : 0;
	}

	//Ends the object: stops the timers and the manager, takes it out of the live set and, with report, calls on_close. Nothing is called afterwards.
	void Terminate(const std::string& reason, const std::string& detail, bool report)
	{
		if(state == State::Closed)
			return;
		auto self = shared_from_this();
		const bool was_connected = connected;
		state = State::Closed;
		connected = false;
		established = false;
		StopTimers();
		if(was_connected)
			pMgr->CloseConnection(current_serial);
		pMgr->Close();
		if(auto ctx = ctx_weak.lock())
			ctx->live.erase(self);
		if(report)
			Call("close",[&](lua_State* L)
				{
					lua_pushstring(L,reason.c_str());
					if(detail.empty())
						return 1;
					lua_pushstring(L,detail.c_str());
					return 2;
				});
		if(handle_ref != LUA_NOREF)
		{
			luaL_unref(port.L,LUA_REGISTRYINDEX,handle_ref);
			handle_ref = LUA_NOREF;
		}
		LogMsg(port.logname,spdlog::level::debug,port.name,kind+" "+desc+" closed: "+reason);
	}

private:
	SockObj(const PortInfo& port, std::weak_ptr<NetCtx> ctx, const SockOpts& opts):
		port(port),
		ctx_weak(std::move(ctx)),
		opts(opts),
		kind(opts.tls ? "TLSSocket" : "TCPSocket"),
		desc(opts.host+":"+opts.port)
	{
		++LiveObjects;
	}

	void Init()
	{
		auto weak = weak_from_this();
		auto read_cb = [weak](odc::buf_t& buf)
				   {
					   std::string data(asio::buffers_begin(buf.data()),asio::buffers_end(buf.data()));
					   buf.consume(buf.size());
					   if(auto self = weak.lock())
						   self->Hop([d{std::move(data)}](SockObj& o){ o.OnData(d); });
				   };
		auto log_cb = [logname{port.logname},name{port.name},kind{kind},desc{desc}](const std::string& level, const std::string& msg)
				  {
					  LogMsg(logname,spdlog::level::from_str(level),name,kind+" "+desc+": "+msg);
				  };
		if(opts.tls)
			pMgr = std::make_unique<odc::TCPSessionManager>(odc::asio_service::Get(),opts.server,opts.host,opts.port,opts.tls_context,opts.peer_name,
				read_cb,[](bool){},std::numeric_limits<size_t>::max(),opts.auto_reopen,opts.retry_ms,opts.throttle_bitrate,opts.throttle_chunksize,
				opts.throttle_writedelay_ms,log_cb,opts.keepalive,opts.keepalive_idle_s,opts.keepalive_retry_s,opts.keepalive_count);
		else
			pMgr = std::make_unique<odc::TCPSessionManager>(odc::asio_service::Get(),opts.server,opts.host,opts.port,read_cb,[](bool){},
				opts.write_buffer_limit,opts.auto_reopen,opts.retry_ms,opts.throttle_bitrate,opts.throttle_chunksize,opts.throttle_writedelay_ms,log_cb,
				opts.keepalive,opts.keepalive_idle_s,opts.keepalive_retry_s,opts.keepalive_count);
		//the events of the sessions, not the state callback (which says nothing about a TLS handshake that fails)
		odc::TCPSessionManager::SessionEvents events;
		events.onConnection = [weak](bool up, uint64_t serial)
					    {
						    if(auto self = weak.lock())
							    self->Hop([=](SockObj& o){ o.OnConnection(up,serial); });
					    };
		if(opts.tls)
		{
			events.onEstablished = [weak](const odc::tls::PeerInfo& peer, uint64_t serial)
						     {
							     if(auto self = weak.lock())
								     self->Hop([=](SockObj& o){ o.OnEstablished(peer,serial); });
						     };
			events.onEnded = [weak](const odc::TCPSessionManager::SessionEnd& end)
					     {
						     if(auto self = weak.lock())
							     self->Hop([=](SockObj& o){ o.OnSessionEnd(end); });
					     };
			for(const auto& w : opts.tls_context->Warnings())
				LogMsg(port.logname,spdlog::level::warn,port.name,kind+" "+desc+": "+w);
		}
		else if(opts.server && !IsLoopback(opts.host))
			LogMsg(port.logname,spdlog::level::warn,port.name,kind+" "+desc+": listening on a non-loopback address, without TLS.");
		pMgr->SetSessionEvents(events);
	}

	//From any thread: run f on the Lua strand, if the port is still alive and the object not ended
	template<typename F>
	void Hop(F&& f)
	{
		auto self = weak_from_this().lock();
		auto strand = port.strand.lock();
		auto tracker = port.tracker.lock();
		if(!self || !strand || !tracker)
			return;
		strand->post([self,tracker,f{std::forward<F>(f)}]() mutable
			{
				if(self->state != State::Closed)
					f(*self);
			});
	}

	//Calls a callback of the script: on_<event>(sock, ...) with the arguments pushed by push (returning how many it pushed).
	//An error is logged, and the object carries on.
	template<typename F>
	void Call(const char* event, F&& push)
	{
		if(handle_ref == LUA_NOREF)
			return;
		lua_State* L = port.L;
		const int top = lua_gettop(L);
		if(!lua_checkstack(L,8))
			return;
		lua_rawgeti(L,LUA_REGISTRYINDEX,handle_ref);
		lua_getiuservalue(L,-1,1);
		if(!lua_istable(L,-1))
		{
			lua_settop(L,top);
			return;
		}
		lua_getfield(L,-1,event);
		if(!lua_isfunction(L,-1))
		{
			lua_settop(L,top);
			return;
		}
		lua_pushvalue(L,top+1);
		const int nargs = 1 + push(L);
		if(lua_pcall(L,nargs,0,0) != LUA_OK)
		{
			std::string err = lua_isstring(L,-1) ? lua_tostring(L,-1) : "(error object is not a string)";
			LogMsg(port.logname,spdlog::level::err,port.name,"Error calling "+kind+" "+desc+" on_"+event+" callback: "+err);
		}
		lua_settop(L,top);
	}

	//the callbacks that take a reason and a detail (the detail is left out when there is none, unless always_detail)
	void CallReason(const char* event, const std::string& reason, const std::string& detail, bool always_detail = false)
	{
		Call(event,[&](lua_State* L)
			{
				lua_pushstring(L,reason.c_str());
				if(detail.empty() && !always_detail)
					return 1;
				lua_pushstring(L,detail.c_str());
				return 2;
			});
	}

	static void PushPeer(lua_State* L, const odc::tls::PeerInfo& p)
	{
		lua_createtable(L,0,11);
		auto str = [L](const char* k, const std::string& v){ lua_pushlstring(L,v.data(),v.size()); lua_setfield(L,-2,k); };
		str("protocol",p.protocol);
		str("cipher",p.cipher);
		lua_pushboolean(L,p.resumed);
		lua_setfield(L,-2,"resumed");
		lua_pushboolean(L,p.certificate);
		lua_setfield(L,-2,"certificate");
		str("subject",p.subject);
		str("issuer",p.issuer);
		str("serial",p.serial);
		str("sha256",p.sha256);
		str("not_before",p.notBefore);
		str("not_after",p.notAfter);
		lua_pushinteger(L,static_cast<lua_Integer>(p.secondsToExpiry));
		lua_setfield(L,-2,"seconds_to_expiry");
	}

	//---- events (Lua strand, object not Closed)
	void OnConnection(bool up, uint64_t serial)
	{
		if(up)
		{
			if(state != State::Opened)
				return;
			connected = true;
			established = false;
			session_ended = false;
			current_serial = serial;
			pending_reason.clear();
			pending_detail.clear();
			++connect_gen;
			if(opts.tls)
			{
				//(the handshake has its own timeout, HandshakeTimeoutms; the idle timeout starts with the session)
				if(!opts.tls_conf.verifyPeer)
					LogMsg(port.logname,spdlog::level::warn,port.name,kind+" "+desc+": the peer's certificate is not verified (VerifyPeer is false).");
				if(!opts.server && !opts.tls_conf.verifyHostname)
					LogMsg(port.logname,spdlog::level::warn,port.name,kind+" "+desc+": the server's name is not verified against its certificate (VerifyHostname is false).");
				return;
			}
			if(opts.idle_timeout_ms)
				ArmIdleTimer();
			Call("open",[](lua_State*){ return 0; });
			return;
		}
		if(state != State::Opened || !connected || serial != current_serial)
			return;
		connected = false;
		established = false;
		++idle_gen;
		const std::string reason = pending_reason.empty() ? "closed" : pending_reason;
		const std::string detail = pending_detail;
		pending_reason.clear();
		pending_detail.clear();
		if(!opts.auto_reopen)
		{
			Terminate(reason,detail,true);
			return;
		}
		CallReason("close",reason,detail);
		if(state == State::Opened && !connected)
			ArmConnectTimer();
	}

	//TLS: the handshake of the session of connection serial completed
	void OnEstablished(const odc::tls::PeerInfo& peer, uint64_t serial)
	{
		if(state != State::Opened || !connected || serial != current_serial)
			return;
		established = true;
		last_peer = peer;
		if(opts.idle_timeout_ms)
			ArmIdleTimer();
		Call("open",[&](lua_State* L)
			{
				PushPeer(L,peer);
				return 1;
			});
	}

	//TLS: the session of a connection ended. The end of the connection follows (OnConnection(false)), which is where on_close is called.
	void OnSessionEnd(const odc::TCPSessionManager::SessionEnd& end)
	{
		if(state != State::Opened || !connected || end.serial != current_serial)
			return;
		established = false;
		session_ended = true;
		switch(end.kind)
		{
			case odc::TCPSessionManager::SessionEnd::Kind::Failed:
				pending_reason = "tls_failed";
				pending_detail = end.reason;
				LogMsg(port.logname,spdlog::level::warn,port.name,kind+" "+desc+": the TLS session failed: "+end.reason);
				CallReason("tls_failed","tls_failed",end.reason);
				break;
			case odc::TCPSessionManager::SessionEnd::Kind::PeerClosed:
				if(pending_reason.empty())
					pending_reason = "closed";
				break;
			case odc::TCPSessionManager::SessionEnd::Kind::ConnectionLost:
				if(pending_reason.empty() && !end.handshakeSummary.empty())
					pending_detail = end.reason+" ("+end.handshakeSummary+")";
				break;
			case odc::TCPSessionManager::SessionEnd::Kind::ClosedLocally:
				break;
		}
	}

	void OnData(const std::string& data)
	{
		if(state != State::Opened || !connected)
			return;
		last_activity = std::chrono::steady_clock::now();
		Call("data",[&](lua_State* L)
			{
				lua_pushlstring(L,data.data(),data.size());
				return 1;
			});
	}

	//A connect attempt did not complete in time (Lua strand, Opened, not connected)
	void AttemptFailed(const std::string& reason, const std::string& detail)
	{
		if(!opts.auto_reopen)
		{
			Terminate(reason,detail,true);
			return;
		}
		//start again: Close() ends the attempt in progress (and the manager's waiting), Open() makes a new one
		pMgr->Close();
		pMgr->Open();
		CallReason("error",reason,detail,true);
		if(state == State::Opened && !connected)
			ArmConnectTimer();
	}

	//---- timers
	void StopTimers()
	{
		++connect_gen;
		++idle_gen;
		for(auto* t : {&connect_timer,&idle_timer})
			if(*t)
				(*t)->cancel();
	}

	void StartTimer(std::shared_ptr<odc::steady_timer>& timer, uint64_t ms, uint64_t gen, void (SockObj::* handler)(uint64_t))
	{
		auto strand = port.strand.lock();
		if(!strand)
			return;
		if(!timer)
			timer = odc::asio_service::Get()->make_steady_timer();
		timer->expires_from_now(std::chrono::milliseconds(ms));
		timer->async_wait(strand->wrap([weak{weak_from_this()},tracker_weak{port.tracker},gen,handler](std::error_code ec)
			{
				if(ec)
					return;
				auto self = weak.lock();
				auto tracker = tracker_weak.lock();
				if(self && tracker)
					((*self).*handler)(gen);
			}));
	}

	//The deadline for the connect attempt that is starting
	void ArmConnectTimer()
	{
		if(opts.server || opts.connect_timeout_ms == 0 || state != State::Opened || connected)
			return;
		StartTimer(connect_timer,opts.connect_timeout_ms,++connect_gen,&SockObj::OnConnectTimer);
	}

	void OnConnectTimer(uint64_t gen)
	{
		if(state != State::Opened || connected || gen != connect_gen)
			return;
		AttemptFailed("timeout","connect_timeout_ms");
	}

	void ArmIdleTimer()
	{
		last_activity = std::chrono::steady_clock::now();
		StartTimer(idle_timer,opts.idle_timeout_ms,++idle_gen,&SockObj::OnIdleTimer);
	}

	void OnIdleTimer(uint64_t gen)
	{
		if(state != State::Opened || !connected || gen != idle_gen)
			return;
		const auto idle = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-last_activity).count();
		if(static_cast<uint64_t>(idle) >= opts.idle_timeout_ms)
		{
			if(pending_reason.empty())
			{
				pending_reason = "timeout";
				pending_detail = "idle_timeout_ms";
			}
			//ends this connection only: the end is reported to us as for any other
			pMgr->CloseConnection(current_serial);
			return;
		}
		StartTimer(idle_timer,opts.idle_timeout_ms-static_cast<uint64_t>(idle),gen,&SockObj::OnIdleTimer);
	}

	static int PushTrue(lua_State* L)
	{
		lua_pushboolean(L,1);
		return 1;
	}
	static int PushError(lua_State* L, const std::string& err)
	{
		lua_pushnil(L);
		lua_pushstring(L,err.c_str());
		return 2;
	}

	const PortInfo port;
	const std::weak_ptr<NetCtx> ctx_weak;
	const SockOpts opts;
	const std::string kind;
	const std::string desc;

	//Lua strand only
	State state = State::Created;
	bool connected = false;     //a TCP connection is up
	bool established = false;   //TLS: and its session is established
	bool session_ended = false; //TLS: the session of the current connection has ended (the connection is going)
	uint64_t current_serial = 0;
	int handle_ref = LUA_NOREF;
	std::string pending_reason; //why the connection that is ending ends (set by the first to know)
	std::string pending_detail;
	std::optional<odc::tls::PeerInfo> last_peer;
	std::chrono::steady_clock::time_point last_activity;
	std::shared_ptr<odc::steady_timer> connect_timer;
	std::shared_ptr<odc::steady_timer> idle_timer;
	uint64_t connect_gen = 0;
	uint64_t idle_gen = 0;

	std::unique_ptr<odc::TCPSessionManager> pMgr;
};

struct SockHandle
{
	std::shared_ptr<SockObj> obj;
};

SockObj& Self(lua_State* L)
{
	auto h = static_cast<SockHandle*>(luaL_checkudata(L,1,SockMeta));
	//(a handle always has an object: it is only empty while being constructed)
	return *h->obj;
}

//odc.TCPSocket{...} and odc.TLSSocket{...}: upvalue 1 is the context, upvalue 2 says which
int SockNew(lua_State* L)
{
	auto pCtx = *static_cast<std::shared_ptr<NetCtx>*>(lua_touserdata(L,lua_upvalueindex(1)));
	const bool tls = lua_toboolean(L,lua_upvalueindex(2));
	auto fail = [L](const std::string& error)
			{
				lua_pushnil(L);
				lua_pushstring(L,error.c_str());
				return 2;
			};
	if(!lua_istable(L,1))
		return fail("invalid_options: options");
	if(tls && !odc::tls::TlsSupported())
		return fail("no_tls");
	SockOpts opts;
	opts.tls = tls;
	std::string error;
	try
	{
		const auto bad_option = ParseSockOpts(L,1,opts);
		if(!bad_option.empty())
			error = "invalid_options: "+bad_option;
		else if(tls)
			error = ParseTlsOption(L,1,opts);
	}
	catch(const std::exception& e)
	{
		error = std::string("invalid_options: ")+e.what();
	}
	if(!error.empty())
		return fail(error);

	auto h = static_cast<SockHandle*>(lua_newuserdatauv(L,sizeof(SockHandle),1));
	new(h) SockHandle();
	luaL_setmetatable(L,SockMeta);

	//the callbacks live in the handle, so a handle that is never opened takes them with it when it is collected
	lua_newtable(L);
	for(const char* ev : {"open","data","close","error","tls_failed"})
	{
		lua_getfield(L,1,(std::string("on_")+ev).c_str());
		lua_setfield(L,-2,ev);
	}
	lua_setiuservalue(L,-2,1);

	try
	{
		h->obj = SockObj::Create(pCtx->port,pCtx,opts);
	}
	catch(const std::exception& e)
	{
		lua_pop(L,1);
		return fail(std::string("failed: ")+e.what());
	}
	return 1;
}

int SockGC(lua_State* L)
{
	auto h = static_cast<SockHandle*>(lua_touserdata(L,1));
	std::destroy_at(h);
	return 0;
}

int CtxGC(lua_State* L)
{
	auto p = static_cast<std::shared_ptr<NetCtx>*>(lua_touserdata(L,1));
	std::destroy_at(p);
	return 0;
}

const luaL_Reg SockMethods[] =
{
	{"open",[](lua_State* L) -> int { return Self(L).LuaOpen(L); }},
	{"write",[](lua_State* L) -> int { return Self(L).LuaWrite(L,false); }},
	{"write_to",[](lua_State* L) -> int { return Self(L).LuaWrite(L,true); }},
	{"close",[](lua_State* L) -> int { return Self(L).LuaClose(L); }},
	{"is_open",[](lua_State* L) -> int { lua_pushboolean(L,Self(L).IsOpen()); return 1; }},
	{"serial",[](lua_State* L) -> int { lua_pushinteger(L,static_cast<lua_Integer>(Self(L).Serial())); return 1; }},
	{"peer",[](lua_State* L) -> int { return Self(L).LuaPeer(L); }},
	{"on",[](lua_State* L) -> int { return Self(L).LuaOn(L); }},
	{nullptr,nullptr}
};
} //namespace

void ExportNetWrappers(lua_State* const L,
	std::shared_ptr<odc::strand_t> pSyncStrand,
	std::shared_ptr<void> handler_tracker,
	const std::string& Name,
	const std::string& LogName)
{
	lua_getglobal(L,"odc");
	if(!lua_istable(L,-1))
	{
		lua_pop(L,1);
		return;
	}

	if(luaL_newmetatable(L,CtxMeta))
	{
		lua_pushcfunction(L,CtxGC);
		lua_setfield(L,-2,"__gc");
	}
	lua_pop(L,1);

	if(luaL_newmetatable(L,SockMeta))
	{
		lua_pushcfunction(L,SockGC);
		lua_setfield(L,-2,"__gc");
		lua_newtable(L);
		luaL_setfuncs(L,SockMethods,0);
		lua_setfield(L,-2,"__index");
	}
	lua_pop(L,1);

	auto ctx = std::make_shared<NetCtx>();
	ctx->port = PortInfo{L,pSyncStrand,handler_tracker,Name,LogName};
	auto p = lua_newuserdatauv(L,sizeof(std::shared_ptr<NetCtx>),0);
	new(p) std::shared_ptr<NetCtx>(ctx);
	luaL_setmetatable(L,CtxMeta);
	lua_pushvalue(L,-1);
	lua_setfield(L,LUA_REGISTRYINDEX,CtxKey);
	for(const bool tls : {false,true})
	{
		lua_pushvalue(L,-1);
		lua_pushboolean(L,tls);
		lua_pushcclosure(L,SockNew,2);
		lua_setfield(L,-3,tls ? "TLSSocket" : "TCPSocket");
	}
	lua_pop(L,2);
}

void CloseNetObjects(lua_State* const L, bool permanent)
{
	lua_getfield(L,LUA_REGISTRYINDEX,CtxKey);
	auto p = static_cast<std::shared_ptr<NetCtx>*>(lua_touserdata(L,-1));
	std::shared_ptr<NetCtx> ctx = p ? *p : nullptr;
	lua_pop(L,1);
	if(!ctx)
		return;
	if(permanent)
		ctx->dead = true;
	const auto live = ctx->live; //Terminate() takes each out of the set
	for(const auto& o : live)
		o->Terminate("aborted","",false);
	CloseHttpObjects(L,permanent);
}

size_t NetObjectCount()
{
	return LiveObjects.load()+HttpObjectCount();
}
