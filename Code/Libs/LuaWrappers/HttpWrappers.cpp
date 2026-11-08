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
 * HttpWrappers.cpp
 *
 *      Author: Alan Murray <github@atmurray.net>
 *
 *  odc.HTTPServer: a Simple-Web-Server HTTP or HTTPS server for Lua scripts, running on ODC's io service (its io_context is the server's shared
 *  io_service), so a request is read and answered on the same threads as everything else.
 *  odc.HTTPClient: a Simple-Web-Server HTTP or HTTPS client on the same io service (one SWS client per request, see WebClient), and odc.HTTP, the
 *  pure helpers (percent encoding, query strings, status names, dates).
 *
 *  Threading: SWS calls a route's function on one of those threads. It only copies the request and hops to the Lua strand (holding the port's
 *  handler_tracker, as the socket objects do); the script's handler, res:write() and the timers all run on the Lua strand.
 *
 *  SWS sends a response when the last reference to its Response is released, so a HttpResponse (the native side of the script's 'res') keeps that
 *  reference until the script has written or the response times out (the 504 backstop), whatever the script does with its own references.
 *
 *  Lifetime: like the socket objects. A started server is in the context's 'live' set and its handle is held by a registry reference until it is
 *  stopped; a server is never destroyed on the Lua strand (SWS's destructor waits for its running handlers): the shared_ptr's deleter posts the
 *  destruction, and a stopped SWS server is destroyed on a posted handler too.
 */
#include "HttpWrappers.h"
#include "NetCommon.h"
#include <Lua/CLua.h>
#include <opendatacon/TLSConf.h>
#include <opendatacon/asio.h>
//(SWS after opendatacon/asio.h: it includes standalone asio, which is ODC's)
#include <server_http.hpp>
#include <client_http.hpp>
#ifdef USE_HTTPS
#include <server_https.hpp>
#include <client_https.hpp>
#endif
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

namespace
{
using odcnet::PortInfo;
using odcnet::LogMsg;

//ODC's io_context, for SWS to run on: the io service doesn't hand it out as such, but every object it makes has an executor of it
asio::io_context& IoContext(odc::asio_service& ios)
{
	return static_cast<asio::io_context&>(ios.make_steady_timer()->get_executor().context());
}

const char* const ServerMeta = "odc.HTTPServer";
const char* const ResponseMeta = "odc.HTTPResponse";
const char* const CtxMeta = "odc.HttpContext";
const char* const CtxKey = "odc.HttpContext";
const char* const ClientMeta = "odc.HTTPClient";
const char* const ClientCtxMeta = "odc.HttpClientContext";
const char* const ClientCtxKey = "odc.HttpClientContext";

constexpr uint64_t MaxTimeMs = std::numeric_limits<int32_t>::max();

std::atomic<size_t> LiveServers{0};
std::atomic<size_t> LiveClients{0};

struct HttpOpts
{
	std::string address = "127.0.0.1";
	uint16_t port = 0;
	bool tls = false;
	std::string cert_file;
	std::string key_file;
	std::string ca_file;
	bool verify_peer = true;
	uint64_t request_timeout_ms = 5000;
	uint64_t content_timeout_ms = 60000;
	size_t max_request_bytes = 1048576;
	uint64_t response_timeout_ms = 30000;
};

//What a handler is given, copied out of the SWS request on the thread SWS called us on
struct RequestData
{
	std::string method;
	std::string path; //percent-decoded
	std::string query_string;
	std::vector<std::pair<std::string,std::string>> query; //in order, decoded
	std::string http_version;
	std::vector<std::pair<std::string,std::string>> headers; //lower-case names
	std::string body;
	std::vector<std::string> captures;
	std::string remote_addr;
	unsigned short remote_port = 0;
};

int HexValue(char c)
{
	if(c >= '0' && c <= '9')
		return c-'0';
	if(c >= 'a' && c <= 'f')
		return c-'a'+10;
	if(c >= 'A' && c <= 'F')
		return c-'A'+10;
	return -1;
}

//%XX sequences (and, for a query, '+' as a space); anything that is not a valid sequence is kept as it is
std::string PercentDecode(const std::string& s, bool plus_is_space)
{
	std::string out;
	out.reserve(s.size());
	for(size_t i = 0; i < s.size(); ++i)
	{
		if(s[i] == '%' && i+2 < s.size() && HexValue(s[i+1]) >= 0 && HexValue(s[i+2]) >= 0)
		{
			out += static_cast<char>(HexValue(s[i+1])*16+HexValue(s[i+2]));
			i += 2;
		}
		else if(plus_is_space && s[i] == '+')
			out += ' ';
		else
			out += s[i];
	}
	return out;
}

std::vector<std::pair<std::string,std::string>> ParseQuery(const std::string& qs)
{
	std::vector<std::pair<std::string,std::string>> out;
	size_t pos = 0;
	while(pos <= qs.size())
	{
		auto end = qs.find('&',pos);
		if(end == std::string::npos)
			end = qs.size();
		const auto item = qs.substr(pos,end-pos);
		if(!item.empty())
		{
			const auto eq = item.find('=');
			const auto name = PercentDecode(item.substr(0,eq),true);
			if(!name.empty())
				out.emplace_back(name,eq == std::string::npos ? std::string() : PercentDecode(item.substr(eq+1),true));
		}
		pos = end+1;
	}
	return out;
}

std::string LowerCase(std::string s)
{
	std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){ return static_cast<char>(std::tolower(c)); });
	return s;
}

bool IsToken(const std::string& s)
{
	static const std::string extra = "!#$%&'*+-.^_`|~";
	return !s.empty() && std::all_of(s.begin(),s.end(),[](unsigned char c){ return std::isalnum(c) || extra.find(static_cast<char>(c)) != std::string::npos; });
}

bool IsLoopback(const std::string& address)
{
	std::error_code ec;
	auto a = asio::ip::make_address(address,ec);
	return !ec && a.is_loopback();
}

//An integer (or a float with an integer value) from 0 to max
bool GetUInt(lua_State* L, int idx, const char* name, uint64_t& out, uint64_t max)
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
			out = static_cast<uint64_t>(v);
	}
	lua_pop(L,1);
	return ok;
}

//Is the key file a PEM that is protected with a password? (OpenSSL would ask for it on the console)
bool IsEncryptedKey(const std::string& text)
{
	return text.find("ENCRYPTED") != std::string::npos;
}

bool ReadFile(const std::string& path, std::string& out)
{
	std::ifstream f(path,std::ios::binary);
	if(!f)
		return false;
	out.assign(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());
	return true;
}

//The "tls" option: the restricted set of SWS. Returns "" or what to give the script as the error
std::string ParseTlsOption(lua_State* L, int idx, HttpOpts& o)
{
	idx = lua_absindex(L,idx);
	lua_getfield(L,idx,"tls");
	if(!lua_istable(L,-1))
	{
		lua_pop(L,1);
		return "invalid_options: tls";
	}
	const int tls_idx = lua_gettop(L);
	static const std::set<std::string> allowed{"CertFile","KeyFile","CAFile","VerifyPeer","VerifyHostname"};
	lua_pushnil(L);
	while(lua_next(L,tls_idx))
	{
		std::string key = (lua_type(L,-2) == LUA_TSTRING) ? lua_tostring(L,-2) : "";
		lua_pop(L,1);
		if(key.empty())
		{
			lua_pop(L,2);
			return "invalid_options: tls";
		}
		if(!allowed.count(key))
		{
			lua_pop(L,2);
			return "invalid_options: "+key+" is not supported by the HTTP stack (use odc.TLSSocket)";
		}
	}
	std::string error;
	auto str = [&](const char* name, std::string& out, bool required)
		     {
			     lua_getfield(L,tls_idx,name);
			     const int t = lua_type(L,-1);
			     if(t == LUA_TSTRING && lua_rawlen(L,-1) > 0)
				     out = lua_tostring(L,-1);
			     else if(t != LUA_TNIL || required)
				     error = std::string("invalid_options: tls.")+name;
			     lua_pop(L,1);
		     };
	auto boolean = [&](const char* name, bool& out)
			   {
				   lua_getfield(L,tls_idx,name);
				   const int t = lua_type(L,-1);
				   if(t == LUA_TBOOLEAN)
					   out = lua_toboolean(L,-1);
				   else if(t != LUA_TNIL)
					   error = std::string("invalid_options: tls.")+name;
				   lua_pop(L,1);
			   };
	str("CertFile",o.cert_file,true);
	if(error.empty()) str("KeyFile",o.key_file,true);
	if(error.empty()) str("CAFile",o.ca_file,false);
	bool verify_hostname = true; //(a client's: nothing to check here)
	if(error.empty()) boolean("VerifyPeer",o.verify_peer);
	if(error.empty()) boolean("VerifyHostname",verify_hostname);
	lua_pop(L,1);
	if(!error.empty())
		return error;

	//the files are checked here, with the name of the key: SWS reports a bad one only as a system error
	std::string content;
	if(!ReadFile(o.cert_file,content))
		return "invalid_options: tls.CertFile cannot be read";
	if(!ReadFile(o.key_file,content))
		return "invalid_options: tls.KeyFile cannot be read";
	if(IsEncryptedKey(content))
		return "invalid_options: tls.KeyFile is encrypted: a password protected key is not supported by the HTTP stack (use an unencrypted key, or odc.TLSSocket)";
	if(!o.ca_file.empty() && !ReadFile(o.ca_file,content))
		return "invalid_options: tls.CAFile cannot be read";
	return "";
}

//Returns "" if the options are valid, else what to give the script as the error
std::string ParseHttpOpts(lua_State* L, int idx, HttpOpts& o)
{
	static const std::set<std::string> known
	{
		"address","port","tls","request_timeout_ms","content_timeout_ms","max_request_bytes","response_timeout_ms"
	};
	idx = lua_absindex(L,idx);
	lua_pushnil(L);
	while(lua_next(L,idx))
	{
		std::string key = (lua_type(L,-2) == LUA_TSTRING) ? lua_tostring(L,-2) : "(non-string key)";
		lua_pop(L,1);
		if(!known.count(key))
		{
			lua_pop(L,1);
			return "invalid_options: "+key;
		}
	}
	lua_getfield(L,idx,"address");
	if(!lua_isnil(L,-1))
	{
		std::error_code ec;
		const bool is_string = lua_type(L,-1) == LUA_TSTRING;
		const std::string a = is_string ? lua_tostring(L,-1) : "";
		if(is_string)
			asio::ip::make_address(a,ec);
		if(!is_string || ec)
		{
			lua_pop(L,1);
			return "invalid_options: address";
		}
		o.address = a;
	}
	lua_pop(L,1);
	uint64_t v = o.port;
	if(!GetUInt(L,idx,"port",v,65535)) return "invalid_options: port";
	o.port = static_cast<uint16_t>(v);
	if(!GetUInt(L,idx,"request_timeout_ms",o.request_timeout_ms,MaxTimeMs)) return "invalid_options: request_timeout_ms";
	if(!GetUInt(L,idx,"content_timeout_ms",o.content_timeout_ms,MaxTimeMs)) return "invalid_options: content_timeout_ms";
	if(!GetUInt(L,idx,"response_timeout_ms",o.response_timeout_ms,MaxTimeMs)) return "invalid_options: response_timeout_ms";
	v = o.max_request_bytes;
	if(!GetUInt(L,idx,"max_request_bytes",v,std::numeric_limits<uint32_t>::max()) || v == 0) return "invalid_options: max_request_bytes";
	o.max_request_bytes = static_cast<size_t>(v);

	lua_getfield(L,idx,"tls");
	const bool has_tls = !lua_isnil(L,-1);
	lua_pop(L,1);
	if(has_tls)
	{
		#ifdef USE_HTTPS
		const bool supported = odc::tls::TlsSupported();
		#else
		const bool supported = false;
		#endif
		if(!supported)
			return "no_tls";
		o.tls = true;
		return ParseTlsOption(L,idx,o);
	}
	return "";
}

class HttpServerObj;

//The native side of a script's 'res'
class HttpResponse: public std::enable_shared_from_this<HttpResponse>
{
public:
	using Sender = std::function<void (SimpleWeb::StatusCode, const std::string&, const SimpleWeb::CaseInsensitiveMultimap&, bool)>;

	HttpResponse(Sender sender, std::weak_ptr<HttpServerObj> owner, const PortInfo& port, std::string method, std::string path):
		sender(std::move(sender)),
		owner(std::move(owner)),
		port(port),
		method(std::move(method)),
		path(std::move(path))
	{}

	//A reply that does not go through a script (a server that is stopping): from any thread, once
	void Reply(SimpleWeb::StatusCode code, const std::string& body)
	{
		if(written.exchange(true))
			return;
		if(sender)
			sender(code,body,SimpleWeb::CaseInsensitiveMultimap(),false);
		sender = nullptr;
	}

	//---- Lua strand
	bool Written() const
	{
		return written;
	}
	//Returns "" or the error for the script
	std::string Write(int status, const std::string& body, SimpleWeb::CaseInsensitiveMultimap headers)
	{
		const auto code = static_cast<SimpleWeb::StatusCode>(status);
		if(abandoned)
			return "closed";
		if(written)
			return "already_written";
		if(status <= 0 || !SimpleWeb::status_code_strings().count(code))
			return "invalid_options";
		Send(code,body,std::move(headers));
		return "";
	}
	void CloseAfter()
	{
		close_after = true;
	}
	void ArmTimer(uint64_t ms)
	{
		auto strand = port.strand.lock();
		if(ms == 0 || !strand)
			return;
		timer = odc::asio_service::Get()->make_steady_timer();
		timer->expires_from_now(std::chrono::milliseconds(ms));
		timeout_ms = ms;
		timer->async_wait(strand->wrap([weak{weak_from_this()},tracker_weak{port.tracker}](std::error_code ec)
			{
				if(ec)
					return;
				auto self = weak.lock();
				auto tracker = tracker_weak.lock();
				if(self && tracker)
					self->Expire();
			}));
	}
	//the server is stopping: nothing is sent
	void Abandon()
	{
		abandoned = true;
		written = true;
		if(timer)
			timer->cancel();
		sender = nullptr;
	}

private:
	void Send(SimpleWeb::StatusCode code, const std::string& body, SimpleWeb::CaseInsensitiveMultimap headers);
	void Expire();

	Sender sender;
	const std::weak_ptr<HttpServerObj> owner;
	const PortInfo port;
	const std::string method;
	const std::string path;
	std::atomic<bool> written{false};
	bool abandoned = false;
	bool close_after = false;
	uint64_t timeout_ms = 0;
	std::shared_ptr<odc::steady_timer> timer;
};

//A SWS server of either kind behind one interface
class ServerImpl
{
public:
	virtual ~ServerImpl() = default;
	virtual void AddRoute(const std::string& method, const std::string& pattern, int idx) = 0;
	virtual void AddDefault(const std::string& method, int idx) = 0;
	//Returns "" or the error for the script (with the system's message in detail)
	virtual std::string Start(unsigned short& port, std::string& detail) = 0;
	virtual void Stop() = 0;
};

struct RouteDef
{
	std::string method;
	std::string pattern;
	int idx;
};

enum class State { Created, Started, Stopped };

class HttpServerObj: public std::enable_shared_from_this<HttpServerObj>
{
public:
	struct Context
	{
		PortInfo port;
		std::set<std::shared_ptr<HttpServerObj>> live; //Lua strand only
		bool dead = false;
	};

	static std::shared_ptr<HttpServerObj> Create(const PortInfo& port, std::weak_ptr<Context> ctx, const HttpOpts& opts)
	{
		auto ios = odc::asio_service::Get();
		std::shared_ptr<HttpServerObj> p(new HttpServerObj(port,std::move(ctx),opts), [ios](HttpServerObj* o)
			{
				if(ios->stopped())
					delete o;
				else
					ios->post([up{std::unique_ptr<HttpServerObj>(o)}](){});
			});
		return p;
	}

	~HttpServerObj()
	{
		//(on an io thread: SWS's destructor waits for its handlers)
		if(impl)
			impl->Stop();
		impl.reset();
		LogMsg(port.logname,spdlog::level::debug,port.name,"HTTPServer "+Desc()+" destroyed.");
		--LiveServers;
	}

	//---- the Lua methods (Lua strand)
	int LuaRoute(lua_State* L)
	{
		if(lua_type(L,2) != LUA_TSTRING)
			return PushError(L,"invalid_options: method");
		if(lua_type(L,3) != LUA_TSTRING)
			return PushError(L,"invalid_options: pattern");
		if(!lua_isfunction(L,4))
			return PushError(L,"invalid_options: handler");
		const std::string method(lua_tostring(L,2),lua_rawlen(L,2));
		const std::string pattern(lua_tostring(L,3),lua_rawlen(L,3));
		if(state == State::Started)
			return PushError(L,"already_started");
		if(!IsToken(method) || LowerCase(method) == method)
			return PushError(L,"invalid_options: method");
		if(pattern.empty() || pattern.size() > 1024)
			return PushError(L,"invalid_options: pattern");
		try
		{
			std::regex check(pattern);
		}
		catch(const std::exception& e)
		{
			return PushError(L,std::string("invalid_options: pattern (")+e.what()+")");
		}
		int idx = 0;
		for(auto& r : routes)
			if(r.method == method && r.pattern == pattern)
				idx = r.idx;
		if(idx == 0)
		{
			idx = ++handler_count;
			routes.push_back({method,pattern,idx});
		}
		SetHandler(L,idx,4);
		return PushTrue(L);
	}

	int LuaDefaultRoute(lua_State* L)
	{
		if(!lua_isfunction(L,2))
			return PushError(L,"invalid_options: handler");
		if(state == State::Started)
			return PushError(L,"already_started");
		if(default_idx == 0)
			default_idx = ++handler_count;
		SetHandler(L,default_idx,2);
		return PushTrue(L);
	}

	int LuaStart(lua_State* L)
	{
		if(state == State::Started)
			return PushTrue(L);
		auto ctx = ctx_weak.lock();
		if(!ctx || ctx->dead)
			return PushError(L,"aborted");
		if(!opts.tls && !IsLoopback(opts.address))
			LogMsg(port.logname,spdlog::level::warn,port.name,"HTTPServer "+Desc()+": listening on a non-loopback address, without TLS.");
		std::unique_ptr<ServerImpl> fresh;
		std::string error;
		std::string detail;
		unsigned short bound = 0;
		try
		{
			fresh = MakeImpl(error,detail);
			if(fresh)
			{
				for(const auto& r : routes)
					fresh->AddRoute(r.method,r.pattern,r.idx);
				//(SWS answers nothing to a request no route matches: without a script's default route, say 404)
				for(const char* m : {"GET","POST","PUT","DELETE","PATCH","HEAD","OPTIONS"})
					fresh->AddDefault(m,default_idx ? default_idx : -1);
				error = fresh->Start(bound,detail);
			}
		}
		catch(const std::exception& e)
		{
			error = "failed";
			detail = e.what();
		}
		if(!error.empty())
		{
			DestroyLater(std::move(fresh));
			lua_pushnil(L);
			lua_pushstring(L,error.c_str());
			if(detail.empty())
				return 2;
			lua_pushstring(L,detail.c_str());
			return 3;
		}
		impl = std::move(fresh);
		bound_port = bound;
		state = State::Started;
		ctx->live.insert(shared_from_this());
		lua_pushvalue(L,1);
		handle_ref = luaL_ref(L,LUA_REGISTRYINDEX);
		LogMsg(port.logname,spdlog::level::debug,port.name,"HTTPServer "+Desc()+" started on port "+std::to_string(bound_port)+".");
		return PushTrue(L);
	}

	int LuaStop(lua_State* L)
	{
		Stop(false);
		return PushTrue(L);
	}

	int LuaPort(lua_State* L)
	{
		lua_pushinteger(L,bound_port);
		return 1;
	}

	//Stops the server: no more connections, the connections closed, the unanswered requests dropped, the handle released. No handler is called afterwards.
	void Stop(bool port_closing)
	{
		if(state != State::Started)
			return;
		auto self = shared_from_this();
		state = State::Stopped;
		impl->Stop();
		DestroyLater(std::move(impl));
		for(auto& r : std::set<std::shared_ptr<HttpResponse>>(pending))
			r->Abandon();
		pending.clear();
		if(auto ctx = ctx_weak.lock())
			ctx->live.erase(self);
		if(handle_ref != LUA_NOREF)
		{
			luaL_unref(port.L,LUA_REGISTRYINDEX,handle_ref);
			handle_ref = LUA_NOREF;
		}
		LogMsg(port.logname,spdlog::level::debug,port.name,"HTTPServer "+Desc()+(port_closing ? " stopped (the port is closing)." : " stopped."));
	}

	void Forget(const std::shared_ptr<HttpResponse>& r)
	{
		pending.erase(r);
	}

	//From any SWS thread: the request is for handler idx. The handler is called on the Lua strand.
	void Deliver(int idx, std::shared_ptr<RequestData> request, std::shared_ptr<HttpResponse> response)
	{
		auto self = weak_from_this().lock();
		auto strand = port.strand.lock();
		auto tracker = port.tracker.lock();
		if(!self || !strand || !tracker)
		{
			response->Reply(SimpleWeb::StatusCode::server_error_service_unavailable,"Service Unavailable");
			return;
		}
		strand->post([self,tracker,idx,request,response]()
			{
				self->Dispatch(idx,*request,response);
			});
	}

	const PortInfo& Port() const
	{
		return port;
	}
	std::string Desc() const
	{
		return opts.address+":"+std::to_string(opts.port);
	}

private:
	HttpServerObj(const PortInfo& port, std::weak_ptr<Context> ctx, const HttpOpts& opts):
		port(port),
		ctx_weak(std::move(ctx)),
		opts(opts)
	{
		++LiveServers;
	}

	std::unique_ptr<ServerImpl> MakeImpl(std::string& error, std::string& detail);

	//SWS's destructor waits for its handlers: not on the Lua strand
	static void DestroyLater(std::unique_ptr<ServerImpl> victim)
	{
		if(!victim)
			return;
		auto ios = odc::asio_service::Get();
		if(ios->stopped())
			return;
		ios->post([v{std::move(victim)}](){});
	}

	//handler idx of this server, into the handle's table of functions
	void SetHandler(lua_State* L, int idx, int fn_index)
	{
		lua_getiuservalue(L,1,1);
		lua_pushvalue(L,fn_index);
		lua_rawseti(L,-2,idx);
		lua_pop(L,1);
	}

	void Dispatch(int idx, const RequestData& rd, const std::shared_ptr<HttpResponse>& response)
	{
		if(state != State::Started || handle_ref == LUA_NOREF)
		{
			response->Reply(SimpleWeb::StatusCode::server_error_service_unavailable,"Service Unavailable");
			return;
		}
		pending.insert(response);
		response->ArmTimer(opts.response_timeout_ms);
		lua_State* L = port.L;
		const int top = lua_gettop(L);
		if(!lua_checkstack(L,12))
		{
			Fail(response,"out of Lua stack");
			return;
		}
		lua_rawgeti(L,LUA_REGISTRYINDEX,handle_ref);
		lua_getiuservalue(L,-1,1);
		lua_rawgeti(L,-1,idx);
		if(!lua_isfunction(L,-1))
		{
			lua_settop(L,top);
			Fail(response,"no handler");
			return;
		}
		PushRequest(L,rd);
		auto h = static_cast<std::shared_ptr<HttpResponse>*>(lua_newuserdatauv(L,sizeof(std::shared_ptr<HttpResponse>),0));
		new(h) std::shared_ptr<HttpResponse>(response);
		luaL_setmetatable(L,ResponseMeta);
		if(lua_pcall(L,2,0,0) != LUA_OK)
		{
			std::string err = lua_isstring(L,-1) ? lua_tostring(L,-1) : "(error object is not a string)";
			LogMsg(port.logname,spdlog::level::err,port.name,"Error in HTTPServer "+Desc()+" handler for "+rd.method+" "+rd.path+": "+err);
			lua_settop(L,top);
			Fail(response,"");
			return;
		}
		lua_settop(L,top);
	}

	//the handler raised an error (or there was none): the client gets a 500 if the script has not answered
	void Fail(const std::shared_ptr<HttpResponse>& response, const std::string& why)
	{
		if(!why.empty())
			LogMsg(port.logname,spdlog::level::err,port.name,"HTTPServer "+Desc()+": "+why);
		if(!response->Written())
			response->Write(500,"Internal Server Error",SimpleWeb::CaseInsensitiveMultimap());
	}

	static void PushPairs(lua_State* L, const std::vector<std::pair<std::string,std::string>>& pairs, bool join_duplicates)
	{
		lua_createtable(L,0,static_cast<int>(pairs.size()));
		for(const auto& p : pairs)
		{
			if(join_duplicates)
			{
				lua_getfield(L,-1,p.first.c_str());
				if(lua_isstring(L,-1))
				{
					std::string both = std::string(lua_tostring(L,-1))+", "+p.second;
					lua_pop(L,1);
					lua_pushlstring(L,both.data(),both.size());
					lua_setfield(L,-2,p.first.c_str());
					continue;
				}
				lua_pop(L,1);
			}
			lua_pushlstring(L,p.second.data(),p.second.size());
			lua_setfield(L,-2,p.first.c_str());
		}
	}

	static void PushRequest(lua_State* L, const RequestData& rd)
	{
		auto str = [L](const char* k, const std::string& v){ lua_pushlstring(L,v.data(),v.size()); lua_setfield(L,-2,k); };
		lua_createtable(L,0,12);
		str("method",rd.method);
		str("path",rd.path);
		str("query_string",rd.query_string);
		str("http_version",rd.http_version);
		str("body",rd.body);
		str("remote_addr",rd.remote_addr);
		lua_pushinteger(L,rd.remote_port);
		lua_setfield(L,-2,"remote_port");
		PushPairs(L,rd.headers,true);
		lua_setfield(L,-2,"headers");
		PushPairs(L,rd.query,false); //the last value of a repeated key wins
		lua_setfield(L,-2,"query");
		lua_createtable(L,0,0);
		std::map<std::string,std::vector<std::string>> all;
		for(const auto& q : rd.query)
			all[q.first].push_back(q.second);
		for(const auto& e : all)
		{
			lua_createtable(L,static_cast<int>(e.second.size()),0);
			lua_Integer n = 1;
			for(const auto& v : e.second)
			{
				lua_pushlstring(L,v.data(),v.size());
				lua_rawseti(L,-2,n++);
			}
			lua_setfield(L,-2,e.first.c_str());
		}
		lua_setfield(L,-2,"query_all");
		lua_createtable(L,static_cast<int>(rd.captures.size()),0);
		lua_Integer n = 1;
		for(const auto& c : rd.captures)
		{
			lua_pushlstring(L,c.data(),c.size());
			lua_rawseti(L,-2,n++);
		}
		lua_setfield(L,-2,"captures");
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
	const std::weak_ptr<Context> ctx_weak;
	const HttpOpts opts;

	//Lua strand only
	State state = State::Created;
	int handle_ref = LUA_NOREF;
	int handler_count = 0;
	int default_idx = 0;
	unsigned short bound_port = 0;
	std::vector<RouteDef> routes;
	std::set<std::shared_ptr<HttpResponse>> pending;
	std::unique_ptr<ServerImpl> impl;
};

//---- HttpResponse, which needs the server
void HttpResponse::Send(SimpleWeb::StatusCode code, const std::string& body, SimpleWeb::CaseInsensitiveMultimap headers)
{
	written = true;
	if(timer)
		timer->cancel();
	if(close_after)
	{
		headers.emplace("Connection","close");
		headers.emplace("Content-Length",std::to_string(body.size()));
	}
	if(auto o = owner.lock())
		o->Forget(shared_from_this());
	LogMsg(port.logname,spdlog::level::debug,port.name,"HTTP "+method+" "+path+" -> "+std::to_string(static_cast<int>(code))+" ("+std::to_string(body.size())+" bytes)");
	auto s = std::move(sender);
	sender = nullptr;
	if(s)
		s(code,body,headers,close_after);
}

void HttpResponse::Expire()
{
	if(written)
		return;
	LogMsg(port.logname,spdlog::level::warn,port.name,"HTTP "+method+" "+path+" was not answered within "+std::to_string(timeout_ms)+" ms: answering 504.");
	Send(SimpleWeb::StatusCode::server_error_gateway_timeout,"Gateway Timeout",SimpleWeb::CaseInsensitiveMultimap());
}

//---- the two kinds of SWS server
template<class S>
class TypedServer final: public ServerImpl
{
public:
	template<typename ... Args>
	TypedServer(std::weak_ptr<HttpServerObj> owner, const PortInfo& port, const HttpOpts& opts, Args&& ... args):
		owner(std::move(owner)),
		port(port),
		opts(opts),
		ios(odc::asio_service::Get()),
		server(std::forward<Args>(args)...)
	{}

	void AddRoute(const std::string& method, const std::string& pattern, int idx) override
	{
		using Key = typename decltype(S::resource)::key_type;
		server.resource[Key(pattern)][method] = Handler(idx);
	}
	void AddDefault(const std::string& method, int idx) override
	{
		server.default_resource[method] = Handler(idx);
	}

	std::string Start(unsigned short& bound, std::string& detail) override
	{
		auto& c = server.config;
		c.address = opts.address;
		c.port = opts.port;
		c.timeout_request = static_cast<long>((opts.request_timeout_ms+999)/1000);
		c.timeout_content = static_cast<long>((opts.content_timeout_ms+999)/1000);
		c.max_request_streambuf_size = opts.max_request_bytes;
		#ifdef _WIN32
		c.reuse_address = false; //(here it would let a second server share the port)
		#endif
		//SWS runs on ODC's threads. The io service stays alive as long as the server (the deleter holds it).
		server.io_service = std::shared_ptr<asio::io_context>(&IoContext(*ios),[keep{ios}](asio::io_context*){});
		server.on_error = [port{port}](std::shared_ptr<typename S::Request> request, const SimpleWeb::error_code& ec)
					{
						//(most are a client that went away or sent a bad request)
						LogMsg(port.logname,spdlog::level::debug,port.name,"HTTP request "+(request ? request->method+" "+request->path : std::string("?"))+" ended: "+ec.message());
					};
		try
		{
			server.start();
		}
		catch(const asio::system_error& e)
		{
			detail = e.code().message();
			return e.code() == asio::error::address_in_use ? "address_in_use" : "bind_failed";
		}
		bound = server.LocalPort();
		return "";
	}

	void Stop() override
	{
		server.stop();
	}

private:
	//SWS's server, with the bound port made readable
	struct Exposed: S
	{
		template<typename ... Args>
		explicit Exposed(Args&& ... args): S(std::forward<Args>(args)...) {}
		unsigned short LocalPort()
		{
			try
			{
				return this->acceptor ? this->acceptor->local_endpoint().port() : 0;
			}
			catch(...)
			{
				return 0;
			}
		}
	};

	std::function<void(std::shared_ptr<typename S::Response>, std::shared_ptr<typename S::Request>)> Handler(int idx)
	{
		return [owner{owner},port{port},idx](std::shared_ptr<typename S::Response> res, std::shared_ptr<typename S::Request> req)
			 {
				 auto rd = std::make_shared<RequestData>();
				 rd->method = req->method;
				 rd->path = PercentDecode(req->path,false);
				 rd->query_string = req->query_string;
				 rd->query = ParseQuery(req->query_string);
				 rd->http_version = req->http_version;
				 for(const auto& h : req->header)
					 rd->headers.emplace_back(LowerCase(h.first),h.second);
				 rd->body = req->content.string();
				 for(size_t i = 1; i < req->path_match.size(); ++i)
					 rd->captures.push_back(req->path_match[i].str());
				 const auto remote = req->remote_endpoint();
				 rd->remote_addr = remote.address().to_string();
				 rd->remote_port = remote.port();

				 //SWS sends the response when the last reference is gone: the HttpResponse holds this one until the script has answered
				 auto sender = [res](SimpleWeb::StatusCode code, const std::string& body, const SimpleWeb::CaseInsensitiveMultimap& headers, bool close) mutable
						   {
							   if(!res)
								   return;
							   if(close)
								   res->close_connection_after_response = true;
							   res->write(code,body,headers);
							   res.reset();
						   };
				 auto response = std::make_shared<HttpResponse>(sender,owner,port,rd->method,rd->path);
				 if(idx < 0)
				 {
					 response->Reply(SimpleWeb::StatusCode::client_error_not_found,"Not Found");
					 return;
				 }
				 if(auto o = owner.lock())
					 o->Deliver(idx,rd,response);
				 else
					 response->Reply(SimpleWeb::StatusCode::server_error_service_unavailable,"Service Unavailable");
			 };
	}

	const std::weak_ptr<HttpServerObj> owner;
	const PortInfo port;
	const HttpOpts opts;
	std::shared_ptr<odc::asio_service> ios;
	Exposed server;
};

std::unique_ptr<ServerImpl> HttpServerObj::MakeImpl(std::string& error, std::string& detail)
{
	if(!opts.tls)
		return std::make_unique<TypedServer<SimpleWeb::Server<SimpleWeb::HTTP>>>(weak_from_this(),port,opts);
	#ifdef USE_HTTPS
	try
	{
		//(a CA file is what makes it ask the client for a certificate, and refuse one that does not chain to it)
		return std::make_unique<TypedServer<SimpleWeb::Server<SimpleWeb::HTTPS>>>(weak_from_this(),port,opts,opts.cert_file,opts.key_file,opts.verify_peer ? opts.ca_file : std::string());
	}
	catch(const std::exception& e)
	{
		error = "tls config: could not use CertFile and KeyFile";
		detail = e.what();
		return nullptr;
	}
	#else
	error = "no_tls";
	return nullptr;
	#endif
}

struct ServerHandle
{
	std::shared_ptr<HttpServerObj> obj;
};

HttpServerObj& Self(lua_State* L)
{
	return *static_cast<ServerHandle*>(luaL_checkudata(L,1,ServerMeta))->obj;
}

HttpResponse& Res(lua_State* L)
{
	return **static_cast<std::shared_ptr<HttpResponse>*>(luaL_checkudata(L,1,ResponseMeta));
}

int ServerNew(lua_State* L)
{
	auto pCtx = *static_cast<std::shared_ptr<HttpServerObj::Context>*>(lua_touserdata(L,lua_upvalueindex(1)));
	auto fail = [L](const std::string& error)
			{
				lua_pushnil(L);
				lua_pushstring(L,error.c_str());
				return 2;
			};
	if(!lua_istable(L,1))
		return fail("invalid_options: options");
	HttpOpts opts;
	std::string error;
	try
	{
		error = ParseHttpOpts(L,1,opts);
	}
	catch(const std::exception& e)
	{
		error = std::string("invalid_options: ")+e.what();
	}
	if(!error.empty())
		return fail(error);

	auto h = static_cast<ServerHandle*>(lua_newuserdatauv(L,sizeof(ServerHandle),1));
	new(h) ServerHandle();
	luaL_setmetatable(L,ServerMeta);
	lua_newtable(L); //the handlers, by number
	lua_setiuservalue(L,-2,1);
	try
	{
		h->obj = HttpServerObj::Create(pCtx->port,pCtx,opts);
	}
	catch(const std::exception& e)
	{
		lua_pop(L,1);
		return fail(std::string("failed: ")+e.what());
	}
	return 1;
}

//header names and values from the script: tokens, and no line breaks
bool ReadHeaders(lua_State* L, int idx, SimpleWeb::CaseInsensitiveMultimap& out)
{
	if(lua_isnoneornil(L,idx))
		return true;
	if(!lua_istable(L,idx))
		return false;
	idx = lua_absindex(L,idx);
	bool ok = true;
	lua_pushnil(L);
	while(ok && lua_next(L,idx))
	{
		if(lua_type(L,-2) != LUA_TSTRING || !(lua_type(L,-1) == LUA_TSTRING || lua_type(L,-1) == LUA_TNUMBER))
			ok = false;
		else
		{
			const std::string name = lua_tostring(L,-2);
			const std::string value = lua_tostring(L,-1); //(a number is converted: the key is a string, so lua_next is not confused)
			const std::string lname = LowerCase(name);
			if(!IsToken(name) || lname == "content-length" || lname == "transfer-encoding" || value.find_first_of("\r\n") != std::string::npos)
				ok = false;
			else
				out.emplace(name,value);
		}
		if(ok)
			lua_pop(L,1);
	}
	if(!ok)
		lua_pop(L,2);
	return ok;
}

int ResWrite(lua_State* L)
{
	auto& res = Res(L);
	int isnum = 0;
	auto status = lua_tointegerx(L,2,&isnum);
	if(lua_type(L,2) != LUA_TNUMBER) //(not a numeric string)
		isnum = 0;
	std::string body;
	if(!lua_isnoneornil(L,3))
	{
		if(lua_type(L,3) != LUA_TSTRING)
		{
			lua_pushnil(L);
			lua_pushstring(L,"invalid_options");
			return 2;
		}
		size_t len = 0;
		auto b = lua_tolstring(L,3,&len);
		body.assign(b,len);
	}
	SimpleWeb::CaseInsensitiveMultimap headers;
	if(!isnum || !ReadHeaders(L,4,headers))
	{
		const std::string err = res.Written() ? "already_written" : "invalid_options";
		lua_pushnil(L);
		lua_pushstring(L,err.c_str());
		return 2;
	}
	const auto err = res.Write(static_cast<int>(status),body,std::move(headers));
	if(!err.empty())
	{
		lua_pushnil(L);
		lua_pushstring(L,err.c_str());
		return 2;
	}
	lua_pushboolean(L,1);
	return 1;
}

int ResCloseAfter(lua_State* L)
{
	Res(L).CloseAfter();
	lua_pushboolean(L,1);
	return 1;
}

int ServerGC(lua_State* L)
{
	std::destroy_at(static_cast<ServerHandle*>(lua_touserdata(L,1)));
	return 0;
}

int ResponseGC(lua_State* L)
{
	std::destroy_at(static_cast<std::shared_ptr<HttpResponse>*>(lua_touserdata(L,1)));
	return 0;
}

int CtxGC(lua_State* L)
{
	std::destroy_at(static_cast<std::shared_ptr<HttpServerObj::Context>*>(lua_touserdata(L,1)));
	return 0;
}

//==== odc.HTTPClient

struct ClientOpts
{
	std::string host;
	unsigned short port = 0;
	bool https = false;
	uint64_t timeout_ms = 15000;
	uint64_t connect_timeout_ms = 5000;
	uint64_t max_response_bytes = 1048576; //0: no limit
	std::string proxy;                     //"host:port"
	bool system_roots = false;
	std::string cert_file;
	std::string key_file;
	std::string ca_file;
	bool verify_peer = true;
	bool verify_hostname = true;
};

//What the script is given for a request: an HTTP response, or an error (a stable string, and the system's or the library's text as detail)
struct HttpResult
{
	std::string error;
	std::string detail;
	int status = 0;
	std::string reason;
	std::string version;
	std::vector<std::pair<std::string,std::string>> headers; //lower-case names
	std::string body;
};

bool IsHostName(const std::string& s)
{
	return !s.empty() && s.size() <= 253 && std::all_of(s.begin(),s.end(),[](unsigned char c){ return std::isalnum(c) || c == '.' || c == '-' || c == '_'; });
}

//"host:port" (the port only if required or given)
bool ParseHostPort(const std::string& s, bool port_required, std::string& host, unsigned short& port)
{
	const auto colon = s.rfind(':');
	host = s.substr(0,colon);
	if(colon == std::string::npos)
		return !port_required && IsHostName(host);
	const auto digits = s.substr(colon+1);
	if(!IsHostName(host) || digits.empty() || digits.size() > 5 || !std::all_of(digits.begin(),digits.end(),[](unsigned char c){ return std::isdigit(c); }))
		return false;
	const auto v = std::stoul(digits);
	if(v == 0 || v > 65535)
		return false;
	port = static_cast<unsigned short>(v);
	return true;
}

//scheme://host[:port][/]
std::string ParseUrl(const std::string& url, ClientOpts& o)
{
	std::string rest;
	if(url.size() > 7 && LowerCase(url.substr(0,7)) == "http://")
		rest = url.substr(7);
	else if(url.size() > 8 && LowerCase(url.substr(0,8)) == "https://")
	{
		o.https = true;
		rest = url.substr(8);
	}
	else
		return "invalid_options: url (the scheme must be http or https)";
	if(!rest.empty() && rest.back() == '/')
		rest.pop_back();
	if(!rest.empty() && rest.front() == '[')
		return "invalid_options: url (IPv6 literals are not supported)";
	o.port = o.https ? 443 : 80;
	if(!ParseHostPort(rest,false,o.host,o.port))
		return "invalid_options: url (expected scheme://host[:port]; the path is given per request)";
	return "";
}

//The "tls" option of a client: the restricted set of SWS. Returns "" or the error for the script
std::string ParseClientTls(lua_State* L, int idx, ClientOpts& o)
{
	idx = lua_absindex(L,idx);
	lua_getfield(L,idx,"tls");
	if(!lua_istable(L,-1))
	{
		lua_pop(L,1);
		return "invalid_options: tls";
	}
	const int tls_idx = lua_gettop(L);
	static const std::set<std::string> allowed{"CertFile","KeyFile","CAFile","VerifyPeer","VerifyHostname"};
	lua_pushnil(L);
	while(lua_next(L,tls_idx))
	{
		std::string key = (lua_type(L,-2) == LUA_TSTRING) ? lua_tostring(L,-2) : "";
		lua_pop(L,1);
		if(key.empty())
		{
			lua_pop(L,2);
			return "invalid_options: tls";
		}
		if(!allowed.count(key))
		{
			lua_pop(L,2);
			return "invalid_options: "+key+" is not supported by the HTTP stack (use odc.TLSSocket)";
		}
	}
	std::string error;
	auto str = [&](const char* name, std::string& out)
		     {
			     lua_getfield(L,tls_idx,name);
			     const int t = lua_type(L,-1);
			     if(t == LUA_TSTRING && lua_rawlen(L,-1) > 0)
				     out = lua_tostring(L,-1);
			     else if(t != LUA_TNIL)
				     error = std::string("invalid_options: tls.")+name;
			     lua_pop(L,1);
		     };
	auto boolean = [&](const char* name, bool& out)
			   {
				   lua_getfield(L,tls_idx,name);
				   const int t = lua_type(L,-1);
				   if(t == LUA_TBOOLEAN)
					   out = lua_toboolean(L,-1);
				   else if(t != LUA_TNIL)
					   error = std::string("invalid_options: tls.")+name;
				   lua_pop(L,1);
			   };
	str("CertFile",o.cert_file);
	if(error.empty()) str("KeyFile",o.key_file);
	if(error.empty()) str("CAFile",o.ca_file);
	if(error.empty()) boolean("VerifyPeer",o.verify_peer);
	if(error.empty()) boolean("VerifyHostname",o.verify_hostname);
	lua_pop(L,1);
	if(!error.empty())
		return error;
	if(o.cert_file.empty() != o.key_file.empty())
		return std::string("invalid_options: tls.")+(o.cert_file.empty() ? "CertFile (KeyFile is given without it)" : "KeyFile (CertFile is given without it)");
	std::string content;
	if(!o.cert_file.empty() && !ReadFile(o.cert_file,content))
		return "invalid_options: tls.CertFile cannot be read";
	if(!o.key_file.empty())
	{
		if(!ReadFile(o.key_file,content))
			return "invalid_options: tls.KeyFile cannot be read";
		if(IsEncryptedKey(content))
			return "invalid_options: tls.KeyFile is encrypted: a password protected key is not supported by the HTTP stack (use an unencrypted key, or odc.TLSSocket)";
	}
	if(!o.ca_file.empty() && !ReadFile(o.ca_file,content))
		return "invalid_options: tls.CAFile cannot be read";
	return "";
}

//Returns "" if the options are valid, else what to give the script as the error
std::string ParseClientOpts(lua_State* L, int idx, ClientOpts& o)
{
	static const std::set<std::string> known
	{
		"url","timeout_ms","connect_timeout_ms","max_response_bytes","proxy","tls","system_roots"
	};
	idx = lua_absindex(L,idx);
	lua_pushnil(L);
	while(lua_next(L,idx))
	{
		std::string key = (lua_type(L,-2) == LUA_TSTRING) ? lua_tostring(L,-2) : "(non-string key)";
		lua_pop(L,1);
		if(!known.count(key))
		{
			lua_pop(L,1);
			return "invalid_options: "+key;
		}
	}
	lua_getfield(L,idx,"url");
	if(lua_type(L,-1) != LUA_TSTRING)
	{
		lua_pop(L,1);
		return "invalid_options: url";
	}
	const std::string url(lua_tostring(L,-1),lua_rawlen(L,-1));
	lua_pop(L,1);
	auto error = ParseUrl(url,o);
	if(!error.empty())
		return error;
	if(o.https)
	{
		#ifdef USE_HTTPS
		const bool supported = odc::tls::TlsSupported();
		#else
		const bool supported = false;
		#endif
		if(!supported)
			return "no_tls";
	}
	if(!GetUInt(L,idx,"timeout_ms",o.timeout_ms,MaxTimeMs)) return "invalid_options: timeout_ms";
	if(!GetUInt(L,idx,"connect_timeout_ms",o.connect_timeout_ms,MaxTimeMs)) return "invalid_options: connect_timeout_ms";
	if(!GetUInt(L,idx,"max_response_bytes",o.max_response_bytes,std::numeric_limits<uint32_t>::max())) return "invalid_options: max_response_bytes";

	lua_getfield(L,idx,"system_roots");
	const int sr_type = lua_type(L,-1);
	o.system_roots = lua_toboolean(L,-1);
	lua_pop(L,1);
	if(sr_type != LUA_TNIL && sr_type != LUA_TBOOLEAN)
		return "invalid_options: system_roots";
	lua_getfield(L,idx,"proxy");
	const int proxy_type = lua_type(L,-1);
	const std::string proxy = proxy_type == LUA_TSTRING ? std::string(lua_tostring(L,-1),lua_rawlen(L,-1)) : "";
	lua_pop(L,1);
	if(proxy_type != LUA_TNIL)
	{
		std::string h;
		unsigned short p = 0;
		if(proxy_type != LUA_TSTRING || !ParseHostPort(proxy,true,h,p))
			return "invalid_options: proxy (expected host:port)";
		if(o.https)
			return "invalid_options: proxy (not supported with https)";
		o.proxy = proxy;
	}

	lua_getfield(L,idx,"tls");
	const bool has_tls = !lua_isnil(L,-1);
	lua_pop(L,1);
	if(o.https)
	{
		if(has_tls)
		{
			error = ParseClientTls(L,idx,o);
			if(!error.empty())
				return error;
		}
		if(o.ca_file.empty() && !o.system_roots && o.verify_peer)
			return "invalid_options: tls.CAFile (required for https unless system_roots = true)";
		return "";
	}
	if(has_tls)
		return "invalid_options: tls (only for https urls)";
	if(o.system_roots)
		return "invalid_options: system_roots (only for https urls)";
	return "";
}

//Where a request got to, shared by the SWS client and what it calls back. All of it is written on SWS's threads.
struct ClientShared
{
	std::atomic<int> verify_error{0}; //the first certificate verify error (X509_V_ERR_*), from the verify callback
	std::atomic<bool> connect_timed_out{false};
	std::atomic<uint64_t> expected{std::numeric_limits<uint64_t>::max()}; //the Content-Length, if there was one
	std::atomic<bool> finished{false};
	std::atomic<bool> stopped{false}; //Stop() was called
};

//The stable error string for what SWS reports, from the error codes (never the text). The verify result of OpenSSL is what tells a TLS failure's cause.
void MapError(const std::error_code& ec, const ClientShared& sh, HttpResult& r)
{
	r.detail = ec.message();
	//(the timer closed the connection under the operation in progress: what that reports is up to the platform, 'operation aborted' or a bad file descriptor etc)
	if(sh.connect_timed_out)
	{
		r.error = "timeout";
		r.detail = "the connect_timeout_ms passed before the connection (and the TLS handshake) was up";
		return;
	}
	if(ec == asio::error::operation_aborted)
	{
		r.error = "aborted";
		return;
	}
	//(on Windows asio's netdb and addrinfo categories are the system category, so the lookup errors are matched by code)
	if(ec == asio::error::host_not_found || ec == asio::error::host_not_found_try_again || ec == asio::error::no_data || ec == asio::error::no_recovery || ec == asio::error::service_not_found)
		r.error = "resolve_failed";
	else if(ec == asio::error::connection_refused)
		r.error = "refused";
	else if(ec == asio::error::network_unreachable || ec == asio::error::host_unreachable || ec == asio::error::network_down)
		r.error = "unreachable";
	else if(ec == asio::error::timed_out)
		r.error = "timeout";
	else if(ec == asio::error::eof || ec == asio::error::connection_reset || ec == asio::error::connection_aborted || ec == asio::error::broken_pipe || ec == asio::error::shut_down)
		r.error = "closed";
	else if(ec == std::errc::message_size || ec == asio::error::not_found || ec == asio::error::no_buffer_space)
		r.error = "message_size";
	else if(ec == std::errc::protocol_error)
		r.error = "bad_response";
	#ifdef USE_HTTPS
	else if(ec.category() == asio::error::get_ssl_category())
	{
		r.error = "tls_handshake_failed";
		switch(sh.verify_error.load())
		{
			case 0:
			{
				//no verify error: the peer's alert (SSL_R_*ALERT_* is SSL_AD_REASON_OFFSET plus the alert) says it did not like our certificate
				const auto code = static_cast<unsigned long>(ec.value());
				if(ERR_GET_LIB(code) == ERR_LIB_SSL && ERR_GET_REASON(code) >= SSL_AD_REASON_OFFSET)
				{
					switch(ERR_GET_REASON(code)-SSL_AD_REASON_OFFSET)
					{
						case SSL_AD_BAD_CERTIFICATE:
						case SSL_AD_UNSUPPORTED_CERTIFICATE:
						case SSL_AD_CERTIFICATE_REVOKED:
						case SSL_AD_CERTIFICATE_EXPIRED:
						case SSL_AD_CERTIFICATE_UNKNOWN:
						case SSL_AD_UNKNOWN_CA:
						case SSL_AD_ACCESS_DENIED:
						case 116: //certificate_required (TLS 1.3)
							r.error = "tls_client_certificate_rejected";
							break;
						default:
							break;
					}
				}
				break;
			}
			case X509_V_ERR_CERT_HAS_EXPIRED:
			case X509_V_ERR_CERT_NOT_YET_VALID:
				r.error = "tls_certificate_expired";
				break;
			case X509_V_ERR_HOSTNAME_MISMATCH:
			case X509_V_ERR_IP_ADDRESS_MISMATCH:
				r.error = "tls_hostname_mismatch";
				break;
			default:
				r.error = "tls_untrusted";
				break;
		}
	}
	#endif
	else
		r.error = "failed";
}

int StatusOf(const std::string& status_code)
{
	if(status_code.size() < 3 || !std::isdigit(static_cast<unsigned char>(status_code[0])) || !std::isdigit(static_cast<unsigned char>(status_code[1])) || !std::isdigit(static_cast<unsigned char>(status_code[2]))
	   || (status_code.size() > 3 && status_code[3] != ' '))
		return 0;
	return std::stoi(status_code.substr(0,3));
}

//An SWS client of either kind behind one interface. One per request.
class ClientImpl
{
public:
	virtual ~ClientImpl() = default;
	using Done = std::function<void (HttpResult)>;
	//Starts the request; done is called once, on an SWS thread
	virtual void Send(const std::string& method, const std::string& path, const std::string& body, SimpleWeb::CaseInsensitiveMultimap headers, Done done) = 0;
	virtual void Stop() = 0;
};

//SWS's client, with the connection set up here instead of in SWS (the same steps) for what SWS does not give: no retry of a request on a connection that
//failed (SWS repeats it once), an exact connect timeout, the verify result of a TLS failure, a response without a body (HEAD, 204, 304) and a Content-Length that
//is not a number (SWS's parser throws, which on ODC's threads would stop opendatacon).
template<class S>
class WebClient final: public ClientImpl, public SimpleWeb::Client<S>
{
	using Base = SimpleWeb::Client<S>;
	using Session = typename SimpleWeb::ClientBase<S>::Session;
	using Connection = typename SimpleWeb::ClientBase<S>::Connection;
	using Response = typename SimpleWeb::ClientBase<S>::Response;

public:
	template<typename ... Args>
	WebClient(const ClientOpts& o, Args&& ... args):
		Base(std::forward<Args>(args)...),
		opts(o),
		ios(odc::asio_service::Get()),
		shared(std::make_shared<ClientShared>())
	{
		//SWS runs on ODC's threads. The io service stays alive as long as the client (the deleter holds it).
		this->io_service = std::shared_ptr<asio::io_context>(&IoContext(*ios),[keep{ios}](asio::io_context*){});
		this->config.max_response_streambuf_size = o.max_response_bytes ? static_cast<size_t>(o.max_response_bytes) : std::numeric_limits<size_t>::max();
		this->config.proxy_server = o.proxy;
	}

	//(the HTTPS client only) the verification that records why it failed
	void ConfigureTls()
	{
		#ifdef USE_HTTPS
		if constexpr(std::is_same<S,SimpleWeb::HTTPS>::value)
		{
			if(!opts.verify_peer)
			{
				this->context.set_verify_mode(asio::ssl::verify_none);
				return;
			}
			if(opts.system_roots)
				this->context.set_default_verify_paths();
			this->context.set_verify_mode(asio::ssl::verify_peer);
			this->context.set_verify_callback([sh{shared},check{opts.verify_hostname},host{this->host}](bool preverified, asio::ssl::verify_context& vc)
				{
					if(!preverified)
					{
						int expected = 0;
						sh->verify_error.compare_exchange_strong(expected,X509_STORE_CTX_get_error(vc.native_handle()));
						return false;
					}
					if(!check)
						return true;
					#if ASIO_VERSION >= 103300
					asio::ssl::host_name_verification verify(host);
					#else
					asio::ssl::rfc2818_verification verify(host);
					#endif
					if(verify(true,vc))
						return true;
					int expected = 0;
					sh->verify_error.compare_exchange_strong(expected,X509_V_ERR_HOSTNAME_MISMATCH);
					return false;
				});
		}
		#endif
	}

	void Send(const std::string& method, const std::string& path, const std::string& body, SimpleWeb::CaseInsensitiveMultimap headers, Done done) override
	{
		head = (method == "HEAD");
		headers.emplace("Connection","close");
		//(SWS leaves the length out when there is no body)
		if(body.empty() && (method == "POST" || method == "PUT" || method == "PATCH"))
			headers.emplace("Content-Length","0");
		this->request(method,path,body,headers,[sh{shared},done{std::move(done)}](std::shared_ptr<Response> response, const SimpleWeb::error_code& ec)
			{
				//(a response over the limit is reported more than once, and a stream gives more than one call: the first one counts)
				if(sh->finished.exchange(true))
					return;
				done(Result(*response,ec,*sh));
				if(!response->content.end)
					response->close();
			});
	}

	void Stop() override
	{
		shared->stopped = true;
		Base::stop();
	}

private:
	static HttpResult Result(Response& response, const SimpleWeb::error_code& ec, const ClientShared& sh)
	{
		HttpResult r;
		if(!response.content.end)
		{
			r.error = "message_size";
			r.detail = "the response is larger than max_response_bytes";
			return r;
		}
		if(ec)
		{
			MapError(ec,sh,r);
			return r;
		}
		r.status = StatusOf(response.status_code);
		if(r.status == 0)
		{
			r.error = "bad_response";
			r.detail = "the status line has no status code";
			return r;
		}
		r.reason = response.status_code.size() > 4 ? response.status_code.substr(4) : "";
		r.version = response.http_version;
		for(const auto& h : response.header)
			r.headers.emplace_back(LowerCase(h.first),h.second);
		r.body = response.content.string();
		const auto expected = sh.expected.load();
		if(r.body.size() > expected)
			r.body.resize(static_cast<size_t>(expected));
		return r;
	}

	//The deadline for the resolve, connect and handshake together, which closes the connection
	void ArmConnectTimer(const std::shared_ptr<Connection>& connection)
	{
		if(opts.connect_timeout_ms == 0)
			return;
		auto timer = std::make_unique<asio::steady_timer>(*this->io_service);
		timer->expires_after(std::chrono::milliseconds(opts.connect_timeout_ms));
		timer->async_wait([weak{std::weak_ptr<Connection>(connection)},sh{shared}](const SimpleWeb::error_code& ec)
			{
				if(ec)
					return;
				if(auto c = weak.lock())
				{
					sh->connect_timed_out = true;
					c->close();
				}
			});
		connection->timer = std::move(timer);
	}

	void Fail(const std::shared_ptr<Session>& session, const SimpleWeb::error_code& ec)
	{
		session->connection->cancel_timeout();
		session->callback(ec);
	}

	//Stop() closes the connections it knows, but one that is still being set up (the name not resolved, the socket not open) cannot be closed: it would go
	//on, send the request and wait for the answer to a request nobody wants, holding the io service. Each step that finishes asks.
	bool Stopped(const std::shared_ptr<Session>& session)
	{
		if(!shared->stopped)
			return false;
		session->connection->close();
		Fail(session,std::error_code(asio::error::operation_aborted));
		return true;
	}

	//Resolve, connect, handshake, then the request is written
	void connect(const std::shared_ptr<Session>& session) override
	{
		session->connection->attempt_reconnect = false; //(no second try on a failure: the script decides)
		ArmConnectTimer(session->connection);
		auto resolver = std::make_shared<asio::ip::tcp::resolver>(*this->io_service);
		SimpleWeb::async_resolve(*resolver,*this->host_port,[this,session,resolver](const SimpleWeb::error_code& ec, SimpleWeb::resolver_results results)
			{
				auto lock = session->connection->handler_runner->continue_lock();
				if(!lock)
					return;
				if(Stopped(session))
					return;
				if(ec)
				{
					Fail(session,ec);
					return;
				}
				asio::async_connect(session->connection->socket->lowest_layer(),results,[this,session,resolver](const SimpleWeb::error_code& ec, SimpleWeb::async_connect_endpoint)
					{
						auto lock = session->connection->handler_runner->continue_lock();
						if(!lock)
							return;
						if(Stopped(session))
							return;
						if(ec)
						{
							Fail(session,ec);
							return;
						}
						SimpleWeb::error_code ignored;
						session->connection->socket->lowest_layer().set_option(asio::ip::tcp::no_delay(true),ignored);
						Handshake(session);
					});
			});
	}

	void Handshake(const std::shared_ptr<Session>& session)
	{
		#ifdef USE_HTTPS
		if constexpr(std::is_same<S,SimpleWeb::HTTPS>::value)
		{
			//(no server name for an address)
			SimpleWeb::error_code ec;
			asio::ip::make_address(this->host,ec);
			if(ec)
				SSL_set_tlsext_host_name(session->connection->socket->native_handle(),this->host.c_str());
			session->connection->socket->async_handshake(asio::ssl::stream_base::client,[this,session](const SimpleWeb::error_code& ec)
				{
					auto lock = session->connection->handler_runner->continue_lock();
					if(!lock)
						return;
					if(Stopped(session))
						return;
					if(ec)
						Fail(session,ec);
					else
					{
						session->connection->cancel_timeout();
						Write(session);
					}
				});
			return;
		}
		#endif
		session->connection->cancel_timeout();
		Write(session);
	}

	void Write(const std::shared_ptr<Session>& session)
	{
		asio::async_write(*session->connection->socket,session->request_streambuf->data(),[this,session](const SimpleWeb::error_code& ec, std::size_t)
			{
				auto lock = session->connection->handler_runner->continue_lock();
				if(!lock)
					return;
				if(Stopped(session))
					return;
				if(ec)
					session->callback(ec);
				else
					Read(session);
			});
	}

	//SWS keeps the response's buffer private; it is what the response's content stream reads
	static asio::streambuf& Buf(Response& response)
	{
		return *static_cast<asio::streambuf*>(response.content.rdbuf());
	}

	static bool ParseLength(const std::string& text, uint64_t& out)
	{
		size_t b = 0;
		size_t e = text.size();
		while(b < e && (text[b] == ' ' || text[b] == '\t'))
			++b;
		while(e > b && (text[e-1] == ' ' || text[e-1] == '\t'))
			--e;
		if(b == e || e-b > 19)
			return false;
		uint64_t v = 0;
		for(size_t i = b; i < e; ++i)
		{
			if(!std::isdigit(static_cast<unsigned char>(text[i])))
				return false;
			v = v*10+static_cast<uint64_t>(text[i]-'0');
		}
		out = v;
		return true;
	}

	void Read(const std::shared_ptr<Session>& session)
	{
		asio::async_read_until(*session->connection->socket,Buf(*session->response),SimpleWeb::HeaderEndMatch(),[this,session](const SimpleWeb::error_code& ec, std::size_t bytes_transferred)
			{
				auto lock = session->connection->handler_runner->continue_lock();
				if(!lock)
					return;
				if(ec)
				{
					session->callback(ec);
					return;
				}
				auto& r = *session->response;
				const std::size_t extra = Buf(r).size()-bytes_transferred;
				if(!SimpleWeb::ResponseMessage::parse(r.content,r.http_version,r.status_code,r.header))
				{
					session->callback(SimpleWeb::make_error_code::make_error_code(SimpleWeb::errc::protocol_error));
					return;
				}
				const int status = StatusOf(r.status_code);
				if(status == 0)
				{
					session->callback(SimpleWeb::make_error_code::make_error_code(SimpleWeb::errc::protocol_error));
					return;
				}
				if(head || status < 200 || status == 204 || status == 304)
				{
					Buf(r).consume(Buf(r).size());
					shared->expected = 0;
					session->callback(SimpleWeb::error_code());
					return;
				}
				const auto lengths = r.header.equal_range("Content-Length");
				if(lengths.first != lengths.second)
				{
					uint64_t length = 0;
					for(auto it = lengths.first; it != lengths.second; ++it)
					{
						uint64_t v = 0;
						if(!ParseLength(it->second,v) || (it != lengths.first && v != length))
						{
							session->callback(SimpleWeb::make_error_code::make_error_code(SimpleWeb::errc::protocol_error));
							return;
						}
						length = v;
					}
					shared->expected = length;
					if(opts.max_response_bytes != 0 && length > opts.max_response_bytes)
					{
						session->callback(SimpleWeb::make_error_code::make_error_code(SimpleWeb::errc::message_size));
						return;
					}
					if(length > extra)
						this->read_content(session,static_cast<size_t>(length-extra));
					else
						session->callback(SimpleWeb::error_code());
					return;
				}
				const auto encoding = r.header.find("Transfer-Encoding");
				if(encoding != r.header.end() && LowerCase(encoding->second).find("chunked") != std::string::npos)
				{
					//(the hex number of a chunk size is at most 16 digits, and the bytes already read are moved)
					auto chunk_size_streambuf = std::make_shared<asio::streambuf>(std::max<std::size_t>(16+2,Buf(r).size()));
					auto& source = Buf(r);
					auto& target = *chunk_size_streambuf;
					target.commit(asio::buffer_copy(target.prepare(source.size()),source.data()));
					source.consume(source.size());
					this->read_chunked_transfer_encoded(session,chunk_size_streambuf);
					return;
				}
				//(we asked for the connection to close: the end of the body is the end of the stream)
				this->read_content(session);
			});
	}

	const ClientOpts opts;
	std::shared_ptr<odc::asio_service> ios;
	const std::shared_ptr<ClientShared> shared;
	bool head = false;
};

class HttpClientObj: public std::enable_shared_from_this<HttpClientObj>
{
public:
	struct Context
	{
		PortInfo port;
		std::set<std::shared_ptr<HttpClientObj>> live; //the clients with requests in flight (Lua strand only)
		bool dead = false;
	};

	static std::shared_ptr<HttpClientObj> Create(const PortInfo& port, std::weak_ptr<Context> ctx, const ClientOpts& opts)
	{
		auto ios = odc::asio_service::Get();
		return std::shared_ptr<HttpClientObj>(new HttpClientObj(port,std::move(ctx),opts), [ios](HttpClientObj* o)
			{
				if(ios->stopped())
					delete o;
				else
					ios->post([up{std::unique_ptr<HttpClientObj>(o)}](){});
			});
	}

	~HttpClientObj()
	{
		for(auto& e : inflight)
			if(e.second && e.second->impl)
				e.second->impl->Stop();
		LogMsg(port.logname,spdlog::level::debug,port.name,"HTTPClient "+Desc()+" destroyed.");
		--LiveClients;
	}

	//Builds an SWS client for a request. Returns null, with the error and the library's text, if it cannot be.
	std::unique_ptr<ClientImpl> MakeImpl(std::string& error, std::string& detail) const
	{
		const std::string host_port = opts.host+":"+std::to_string(opts.port);
		try
		{
			if(!opts.https)
				return std::make_unique<WebClient<SimpleWeb::HTTP>>(opts,host_port);
			#ifdef USE_HTTPS
			auto c = std::make_unique<WebClient<SimpleWeb::HTTPS>>(opts,host_port,opts.verify_peer,opts.cert_file,opts.key_file,opts.ca_file);
			c->ConfigureTls();
			return c;
			#else
			error = "no_tls";
			return nullptr;
			#endif
		}
		catch(const std::exception& e)
		{
			error = "tls config: could not use CertFile, KeyFile or CAFile";
			detail = e.what();
			return nullptr;
		}
	}

	//---- the Lua methods (Lua strand)
	int LuaRequest(lua_State* L)
	{
		if(!lua_istable(L,2))
			return PushError(L,"invalid_options: options");
		static const std::set<std::string> known{"method","path","headers","body","on_response"};
		lua_pushnil(L);
		while(lua_next(L,2))
		{
			std::string key = (lua_type(L,-2) == LUA_TSTRING) ? lua_tostring(L,-2) : "(non-string key)";
			lua_pop(L,1);
			if(!known.count(key))
			{
				lua_pop(L,1);
				return PushError(L,"invalid_options: "+key);
			}
		}
		auto text = [L](const char* name, std::string& out) -> bool
				{
					lua_getfield(L,2,name);
					const bool ok = lua_type(L,-1) == LUA_TSTRING;
					if(ok)
						out.assign(lua_tostring(L,-1),lua_rawlen(L,-1));
					lua_pop(L,1);
					return ok;
				};
		std::string method;
		if(!text("method",method) || !IsToken(method) || std::any_of(method.begin(),method.end(),[](unsigned char c){ return std::islower(c); }))
			return PushError(L,"invalid_options: method");
		std::string path = "/";
		lua_getfield(L,2,"path");
		const bool path_given = !lua_isnil(L,-1);
		lua_pop(L,1);
		if(path_given && !text("path",path))
			return PushError(L,"invalid_options: path");
		if(path.empty() || path[0] != '/' || path.size() > 8192 || std::any_of(path.begin(),path.end(),[](unsigned char c){ return c <= ' ' || c == 0x7f; }))
			return PushError(L,"invalid_options: path");
		std::string body;
		lua_getfield(L,2,"body");
		const bool body_given = !lua_isnil(L,-1);
		lua_pop(L,1);
		if(body_given && !text("body",body))
			return PushError(L,"invalid_options: body");
		SimpleWeb::CaseInsensitiveMultimap headers;
		lua_getfield(L,2,"headers");
		const auto header_error = ReadRequestHeaders(L,lua_gettop(L),headers);
		lua_pop(L,1);
		if(!header_error.empty())
			return PushError(L,header_error);
		lua_getfield(L,2,"on_response");
		if(!lua_isfunction(L,-1))
		{
			lua_pop(L,1);
			return PushError(L,"invalid_options: on_response");
		}
		const int cb_ref = luaL_ref(L,LUA_REGISTRYINDEX);

		auto ctx = ctx_weak.lock();
		std::string error;
		std::string detail;
		std::unique_ptr<ClientImpl> impl;
		if(closed)
			error = "closed";
		else if(!ctx || ctx->dead)
			error = "aborted";
		else
			impl = MakeImpl(error,detail);
		if(!impl)
		{
			luaL_unref(L,LUA_REGISTRYINDEX,cb_ref);
			return PushError(L,error,detail);
		}
		auto strand = port.strand.lock();
		if(!strand)
		{
			luaL_unref(L,LUA_REGISTRYINDEX,cb_ref);
			return PushError(L,"aborted");
		}

		auto req = std::make_shared<Req>();
		req->id = ++request_count;
		req->cb_ref = cb_ref;
		req->method = method;
		req->path = path.substr(0,path.find('?')); //(a query string can carry a token: it is not logged)
		req->impl = std::move(impl);
		inflight[req->id] = req;
		ctx->live.insert(shared_from_this());
		if(!opts.verify_peer)
			LogMsg(port.logname,spdlog::level::warn,port.name,"HTTPClient "+Desc()+": VerifyPeer is false, the server's certificate is not checked.");
		else if(opts.https && !opts.verify_hostname)
			LogMsg(port.logname,spdlog::level::warn,port.name,"HTTPClient "+Desc()+": VerifyHostname is false, the name in the server's certificate is not checked.");
		LogMsg(port.logname,spdlog::level::debug,port.name,"HTTPClient "+Desc()+": "+method+" "+req->path+" ("+std::to_string(body.size())+" bytes)");

		if(opts.timeout_ms != 0)
		{
			req->timer = odc::asio_service::Get()->make_steady_timer();
			req->timer->expires_from_now(std::chrono::milliseconds(opts.timeout_ms));
			req->timer->async_wait(strand->wrap([weak{weak_from_this()},tracker_weak{port.tracker},id{req->id}](std::error_code ec)
				{
					if(ec)
						return;
					auto self = weak.lock();
					auto tracker = tracker_weak.lock();
					if(self && tracker)
						self->OnTimeout(id);
				}));
		}
		try
		{
			req->impl->Send(method,path,body,std::move(headers),[id{req->id},weak{weak_from_this()}](HttpResult result)
				{
					Hop(weak,id,std::move(result));
				});
		}
		catch(const std::exception& e)
		{
			HttpResult failed;
			failed.error = "failed";
			failed.detail = e.what();
			Hop(weak_from_this(),req->id,std::move(failed));
		}
		lua_pushboolean(L,1);
		return 1;
	}

	//Aborts what is in flight: each callback is called (with "aborted") once this call has returned
	int LuaCancel(lua_State* L)
	{
		Abort("aborted");
		lua_pushboolean(L,1);
		return 1;
	}

	int LuaClose(lua_State* L)
	{
		closed = true;
		Abort("aborted");
		lua_pushboolean(L,1);
		return 1;
	}

	//The port is closing: nothing is called back, nothing is left
	void CloseForPort()
	{
		closed = true;
		auto self = shared_from_this();
		const auto all = inflight;
		inflight.clear();
		for(const auto& e : all)
		{
			auto& req = *e.second;
			req.done = true;
			if(req.timer)
				req.timer->cancel();
			if(req.impl)
			{
				req.impl->Stop();
				PostDestroy(std::move(req.impl));
			}
			if(req.cb_ref != LUA_NOREF)
				luaL_unref(port.L,LUA_REGISTRYINDEX,req.cb_ref);
		}
		if(auto ctx = ctx_weak.lock())
			ctx->live.erase(self);
		LogMsg(port.logname,spdlog::level::debug,port.name,"HTTPClient "+Desc()+" closed (the port is closing).");
	}

	std::string Desc() const
	{
		return std::string(opts.https ? "https://" : "http://")+opts.host+":"+std::to_string(opts.port);
	}

private:
	struct Req
	{
		uint64_t id = 0;
		int cb_ref = LUA_NOREF;
		std::string method;
		std::string path;
		std::unique_ptr<ClientImpl> impl;
		std::shared_ptr<odc::steady_timer> timer;
		bool done = false; //the result is known (the callback may be waiting for the strand)
		HttpResult result;
	};

	HttpClientObj(const PortInfo& port, std::weak_ptr<Context> ctx, const ClientOpts& opts):
		port(port),
		ctx_weak(std::move(ctx)),
		opts(opts)
	{
		++LiveClients;
	}

	//SWS's destructor waits for its handlers: not on the Lua strand
	template<class T>
	static void PostDestroy(std::unique_ptr<T> victim)
	{
		if(!victim)
			return;
		auto ios = odc::asio_service::Get();
		if(ios->stopped())
			return;
		ios->post([v{std::move(victim)}](){});
	}

	//From an SWS thread: the result of request id goes to the Lua strand
	static void Hop(const std::weak_ptr<HttpClientObj>& weak, uint64_t id, HttpResult result)
	{
		auto self = weak.lock();
		if(!self)
			return;
		auto strand = self->port.strand.lock();
		auto tracker = self->port.tracker.lock();
		if(!strand || !tracker)
			return;
		auto shared_result = std::make_shared<HttpResult>(std::move(result));
		strand->post([self,tracker,id,shared_result]()
			{
				self->OnResult(id,std::move(*shared_result));
			});
	}

	//Settles request id with a result, unless it has been settled (Lua strand)
	bool Settle(uint64_t id, HttpResult result)
	{
		auto it = inflight.find(id);
		if(it == inflight.end() || it->second->done)
			return false;
		auto& req = *it->second;
		req.done = true;
		req.result = std::move(result);
		if(req.timer)
			req.timer->cancel();
		if(req.impl)
		{
			req.impl->Stop();
			PostDestroy(std::move(req.impl));
		}
		return true;
	}

	void OnResult(uint64_t id, HttpResult result)
	{
		if(Settle(id,std::move(result)))
			Deliver(id);
	}

	void OnTimeout(uint64_t id)
	{
		HttpResult r;
		r.error = "timeout";
		r.detail = "no complete response within "+std::to_string(opts.timeout_ms)+" ms";
		OnResult(id,std::move(r));
	}

	void Abort(const std::string& why)
	{
		std::vector<uint64_t> ids;
		for(const auto& e : inflight)
			ids.push_back(e.first);
		auto strand = port.strand.lock();
		auto tracker = port.tracker.lock();
		for(auto id : ids)
		{
			HttpResult r;
			r.error = why;
			if(!Settle(id,std::move(r)) || !strand || !tracker)
				continue;
			//(not from inside this call)
			strand->post([self{shared_from_this()},tracker,id]()
				{
					self->Deliver(id);
				});
		}
	}

	//Calls the script's callback for request id, which is settled (Lua strand)
	void Deliver(uint64_t id)
	{
		auto it = inflight.find(id);
		if(it == inflight.end())
			return;
		auto self = shared_from_this();
		const auto req = it->second;
		inflight.erase(it);
		if(inflight.empty())
			if(auto ctx = ctx_weak.lock())
				ctx->live.erase(self);
		const auto& r = req->result;
		if(r.error.empty())
			LogMsg(port.logname,spdlog::level::debug,port.name,"HTTPClient "+Desc()+": "+req->method+" "+req->path+" -> "+std::to_string(r.status)+" ("+std::to_string(r.body.size())+" bytes)");
		else
			LogMsg(port.logname,spdlog::level::debug,port.name,"HTTPClient "+Desc()+": "+req->method+" "+req->path+" failed: "+r.error+(r.detail.empty() ? "" : " ("+r.detail+")"));

		lua_State* L = port.L;
		const int top = lua_gettop(L);
		if(!lua_checkstack(L,12))
		{
			luaL_unref(L,LUA_REGISTRYINDEX,req->cb_ref);
			LogMsg(port.logname,spdlog::level::err,port.name,"HTTPClient "+Desc()+": out of Lua stack, a callback was not called.");
			return;
		}
		lua_rawgeti(L,LUA_REGISTRYINDEX,req->cb_ref);
		luaL_unref(L,LUA_REGISTRYINDEX,req->cb_ref);
		int nargs = 1;
		if(r.error.empty())
			PushResponse(L,r);
		else
		{
			lua_pushnil(L);
			lua_pushstring(L,r.error.c_str());
			nargs = 2;
			if(!r.detail.empty())
			{
				lua_pushstring(L,r.detail.c_str());
				nargs = 3;
			}
		}
		if(lua_pcall(L,nargs,0,0) != LUA_OK)
		{
			std::string err = lua_isstring(L,-1) ? lua_tostring(L,-1) : "(error object is not a string)";
			LogMsg(port.logname,spdlog::level::err,port.name,"Error in HTTPClient "+Desc()+" on_response for "+req->method+" "+req->path+": "+err);
		}
		lua_settop(L,top);
	}

	static void PushResponse(lua_State* L, const HttpResult& r)
	{
		lua_createtable(L,0,5);
		lua_pushinteger(L,r.status);
		lua_setfield(L,-2,"status");
		lua_pushlstring(L,r.reason.data(),r.reason.size());
		lua_setfield(L,-2,"reason");
		lua_pushlstring(L,r.version.data(),r.version.size());
		lua_setfield(L,-2,"http_version");
		lua_pushlstring(L,r.body.data(),r.body.size());
		lua_setfield(L,-2,"body");
		lua_createtable(L,0,static_cast<int>(r.headers.size()));
		for(const auto& h : r.headers)
		{
			if(h.first == "set-cookie")
			{
				lua_getfield(L,-1,"set-cookie");
				if(!lua_istable(L,-1))
				{
					lua_pop(L,1);
					lua_createtable(L,1,0);
					lua_pushvalue(L,-1);
					lua_setfield(L,-3,"set-cookie");
				}
				lua_pushlstring(L,h.second.data(),h.second.size());
				lua_rawseti(L,-2,static_cast<lua_Integer>(lua_rawlen(L,-2))+1);
				lua_pop(L,1);
				continue;
			}
			lua_getfield(L,-1,h.first.c_str());
			if(lua_isstring(L,-1))
			{
				std::string both = std::string(lua_tostring(L,-1))+", "+h.second;
				lua_pop(L,1);
				lua_pushlstring(L,both.data(),both.size());
			}
			else
			{
				lua_pop(L,1);
				lua_pushlstring(L,h.second.data(),h.second.size());
			}
			lua_setfield(L,-2,h.first.c_str());
		}
		lua_setfield(L,-2,"headers");
	}

	static int PushError(lua_State* L, const std::string& err, const std::string& detail = "")
	{
		lua_pushnil(L);
		lua_pushstring(L,err.c_str());
		if(detail.empty())
			return 2;
		lua_pushstring(L,detail.c_str());
		return 3;
	}

	//Returns "" or the error for the script. Names are tokens, values have no control character (no line break, no NUL), and what the binding sets itself is refused.
	static std::string ReadRequestHeaders(lua_State* L, int idx, SimpleWeb::CaseInsensitiveMultimap& out)
	{
		if(lua_isnoneornil(L,idx))
			return "";
		if(!lua_istable(L,idx))
			return "invalid_options: headers";
		idx = lua_absindex(L,idx);
		static const std::set<std::string> reserved{"host","connection","content-length","transfer-encoding","expect","upgrade"};
		std::string error;
		lua_pushnil(L);
		while(error.empty() && lua_next(L,idx))
		{
			if(lua_type(L,-2) != LUA_TSTRING || !(lua_type(L,-1) == LUA_TSTRING || lua_type(L,-1) == LUA_TNUMBER))
				error = "invalid_options: headers";
			else
			{
				size_t name_len = 0;
				size_t value_len = 0;
				const char* n = lua_tolstring(L,-2,&name_len);
				const char* v = lua_tolstring(L,-1,&value_len); //(a number is converted: the key is a string, so lua_next is not confused)
				const std::string name(n,name_len);
				const std::string value(v,value_len);
				if(!IsToken(name))
					error = "invalid_options: headers (a name is not a valid header name)";
				else if(reserved.count(LowerCase(name)))
					error = "invalid_options: headers."+name+" is set by the binding";
				else if(std::any_of(value.begin(),value.end(),[](unsigned char c){ return (c < 0x20 && c != '\t') || c == 0x7f; }))
					error = "invalid_options: headers."+name+" has a control character in its value";
				else
					out.emplace(name,value);
			}
			if(error.empty())
				lua_pop(L,1);
		}
		if(!error.empty())
			lua_pop(L,2);
		return error;
	}

	const PortInfo port;
	const std::weak_ptr<Context> ctx_weak;
	const ClientOpts opts;

	//Lua strand only
	bool closed = false;
	uint64_t request_count = 0;
	std::map<uint64_t,std::shared_ptr<Req>> inflight;
};

struct ClientHandle
{
	std::shared_ptr<HttpClientObj> obj;
};

HttpClientObj& ClientSelf(lua_State* L)
{
	return *static_cast<ClientHandle*>(luaL_checkudata(L,1,ClientMeta))->obj;
}

int ClientNew(lua_State* L)
{
	auto pCtx = *static_cast<std::shared_ptr<HttpClientObj::Context>*>(lua_touserdata(L,lua_upvalueindex(1)));
	auto fail = [L](const std::string& error, const std::string& detail = "")
			{
				lua_pushnil(L);
				lua_pushstring(L,error.c_str());
				if(detail.empty())
					return 2;
				lua_pushstring(L,detail.c_str());
				return 3;
			};
	if(!lua_istable(L,1))
		return fail("invalid_options: options");
	ClientOpts opts;
	std::string error;
	try
	{
		error = ParseClientOpts(L,1,opts);
	}
	catch(const std::exception& e)
	{
		error = std::string("invalid_options: ")+e.what();
	}
	if(!error.empty())
		return fail(error);

	std::shared_ptr<HttpClientObj> obj;
	try
	{
		obj = HttpClientObj::Create(pCtx->port,pCtx,opts);
	}
	catch(const std::exception& e)
	{
		return fail("failed",e.what());
	}
	//(the files are read when a request is made, so a renewed certificate is used by the next request: they are tried here, for the script to be told now)
	std::string detail;
	auto probe = obj->MakeImpl(error,detail);
	if(!probe)
		return fail(error,detail);
	probe.reset();

	auto h = static_cast<ClientHandle*>(lua_newuserdatauv(L,sizeof(ClientHandle),0));
	new(h) ClientHandle();
	h->obj = std::move(obj);
	luaL_setmetatable(L,ClientMeta);
	return 1;
}

int ClientGC(lua_State* L)
{
	std::destroy_at(static_cast<ClientHandle*>(lua_touserdata(L,1)));
	return 0;
}

int ClientCtxGC(lua_State* L)
{
	std::destroy_at(static_cast<std::shared_ptr<HttpClientObj::Context>*>(lua_touserdata(L,1)));
	return 0;
}

const luaL_Reg ClientMethods[] =
{
	{"request",[](lua_State* L) -> int { return ClientSelf(L).LuaRequest(L); }},
	{"cancel",[](lua_State* L) -> int { return ClientSelf(L).LuaCancel(L); }},
	{"close",[](lua_State* L) -> int { return ClientSelf(L).LuaClose(L); }},
	{nullptr,nullptr}
};

//==== odc.HTTP, the pure helpers

int HelperError(lua_State* L, const char* what)
{
	lua_pushnil(L);
	lua_pushstring(L,(std::string("invalid_options: ")+what).c_str());
	return 2;
}

std::string PercentEncode(const std::string& s)
{
	static const char* const hex = "0123456789ABCDEF";
	std::string out;
	out.reserve(s.size());
	for(unsigned char c : s)
	{
		if(std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~')
			out += static_cast<char>(c);
		else
		{
			out += '%';
			out += hex[c >> 4];
			out += hex[c & 15];
		}
	}
	return out;
}

int HttpPercentEncode(lua_State* L)
{
	if(lua_type(L,1) != LUA_TSTRING)
		return HelperError(L,"s");
	const auto out = PercentEncode(std::string(lua_tostring(L,1),lua_rawlen(L,1)));
	lua_pushlstring(L,out.data(),out.size());
	return 1;
}

//(a '+' stays a '+': it is a space only in a query, see query_parse)
int HttpPercentDecode(lua_State* L)
{
	if(lua_type(L,1) != LUA_TSTRING)
		return HelperError(L,"s");
	const auto out = PercentDecode(std::string(lua_tostring(L,1),lua_rawlen(L,1)),false);
	lua_pushlstring(L,out.data(),out.size());
	return 1;
}

//{ key = value, key = { v1, v2 } } as key=value&..., the keys sorted, a value of a list repeats the key
int HttpQueryEncode(lua_State* L)
{
	if(!lua_istable(L,1))
		return HelperError(L,"query");
	std::map<std::string,std::vector<std::string>> all;
	auto scalar = [L](std::string& out) -> bool
			  {
				  const int t = lua_type(L,-1);
				  if(t == LUA_TSTRING || t == LUA_TNUMBER)
				  {
					  size_t len = 0;
					  auto s = luaL_tolstring(L,-1,&len);
					  out.assign(s,len);
					  lua_pop(L,1);
					  return true;
				  }
				  if(t == LUA_TBOOLEAN)
				  {
					  out = lua_toboolean(L,-1) ? "true" : "false";
					  return true;
				  }
				  return false;
			  };
	lua_pushnil(L);
	while(lua_next(L,1))
	{
		if(lua_type(L,-2) != LUA_TSTRING)
		{
			lua_pop(L,2);
			return HelperError(L,"query (a key is not a string)");
		}
		const std::string key(lua_tostring(L,-2),lua_rawlen(L,-2));
		auto& values = all[key];
		std::string v;
		if(lua_istable(L,-1))
		{
			const auto n = static_cast<lua_Integer>(lua_rawlen(L,-1));
			for(lua_Integer i = 1; i <= n; ++i)
			{
				lua_rawgeti(L,-1,i);
				if(!scalar(v))
				{
					lua_pop(L,3);
					return HelperError(L,("query."+key+" (a list holds only strings, numbers and booleans)").c_str());
				}
				lua_pop(L,1);
				values.push_back(v);
			}
		}
		else if(scalar(v))
			values.push_back(v);
		else
		{
			lua_pop(L,2);
			return HelperError(L,("query."+key+" (not a string, number, boolean or list)").c_str());
		}
		lua_pop(L,1);
	}
	std::string out;
	for(const auto& e : all)
		for(const auto& v : e.second)
		{
			if(!out.empty())
				out += '&';
			out += PercentEncode(e.first)+"="+PercentEncode(v);
		}
	lua_pushlstring(L,out.data(),out.size());
	return 1;
}

//"a=1&b=x%20y" as { a = "1", b = "x y" } (the last value of a key wins), and a second table with a list of the values of each key
int HttpQueryParse(lua_State* L)
{
	if(lua_type(L,1) != LUA_TSTRING)
		return HelperError(L,"s");
	std::string qs(lua_tostring(L,1),lua_rawlen(L,1));
	if(!qs.empty() && qs[0] == '?')
		qs.erase(0,1);
	const auto pairs = ParseQuery(qs);
	lua_createtable(L,0,static_cast<int>(pairs.size()));
	for(const auto& p : pairs)
	{
		lua_pushlstring(L,p.second.data(),p.second.size());
		lua_setfield(L,-2,p.first.c_str());
	}
	std::map<std::string,std::vector<std::string>> all;
	for(const auto& p : pairs)
		all[p.first].push_back(p.second);
	lua_createtable(L,0,static_cast<int>(all.size()));
	for(const auto& e : all)
	{
		lua_createtable(L,static_cast<int>(e.second.size()),0);
		lua_Integer n = 1;
		for(const auto& v : e.second)
		{
			lua_pushlstring(L,v.data(),v.size());
			lua_rawseti(L,-2,n++);
		}
		lua_setfield(L,-2,e.first.c_str());
	}
	return 2;
}

int HttpReason(lua_State* L)
{
	if(lua_type(L,1) != LUA_TNUMBER)
		return HelperError(L,"code");
	int isnum = 0;
	const auto code = lua_tointegerx(L,1,&isnum);
	if(!isnum || code <= 0 || code > 999)
	{
		lua_pushnil(L);
		return 1;
	}
	const auto& strings = SimpleWeb::status_code_strings();
	const auto it = strings.find(static_cast<SimpleWeb::StatusCode>(code));
	if(it == strings.end() || it->second.size() < 5)
	{
		lua_pushnil(L);
		return 1;
	}
	lua_pushlstring(L,it->second.data()+4,it->second.size()-4);
	return 1;
}

//RFC 7231's IMF-fixdate of a time given in milliseconds since 1970 (now if none), in UTC
int HttpDate(lua_State* L)
{
	int64_t secs = 0;
	if(lua_isnoneornil(L,1))
		secs = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	else
	{
		if(lua_type(L,1) != LUA_TNUMBER)
			return HelperError(L,"ms");
		const auto ms = lua_tonumber(L,1);
		if(!(ms >= 0) || ms > 253402300799000.0) //(up to the year 9999)
			return HelperError(L,"ms");
		secs = static_cast<int64_t>(ms/1000.0);
	}
	static const char* const days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
	static const char* const months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
	int64_t z = secs/86400;
	const auto rem = secs%86400;
	const int weekday = static_cast<int>((z+4)%7);
	z += 719468; //days from 0000-03-01
	const int64_t era = z/146097;
	const int64_t doe = z-era*146097;
	const int64_t yoe = (doe-doe/1460+doe/36524-doe/146096)/365;
	const int64_t doy = doe-(365*yoe+yoe/4-yoe/100);
	const int64_t mp = (5*doy+2)/153;
	const int day = static_cast<int>(doy-(153*mp+2)/5+1);
	const int month = static_cast<int>(mp < 10 ? mp+3 : mp-9);
	const int64_t year = yoe+era*400+(month <= 2 ? 1 : 0);
	char buf[64];
	std::snprintf(buf,sizeof(buf),"%s, %02d %s %04d %02d:%02d:%02d GMT",days[weekday],day,months[month-1],static_cast<int>(year),static_cast<int>(rem/3600),static_cast<int>(rem%3600/60),static_cast<int>(rem%60));
	lua_pushstring(L,buf);
	return 1;
}

//The name of a status for odc.HTTP.status: "Not Found" is not_found, "I'm a teapot" is im_a_teapot
std::string StatusName(const std::string& reason)
{
	std::string out;
	for(unsigned char c : reason)
	{
		if(std::isalnum(c))
			out += static_cast<char>(std::tolower(c));
		else if(c != '\'' && !out.empty() && out.back() != '_')
			out += '_';
	}
	while(!out.empty() && out.back() == '_')
		out.pop_back();
	return out;
}

//Creates odc.HTTP (on the stack top's odc table)
void ExportHttpHelpers(lua_State* L)
{
	static const luaL_Reg functions[] =
	{
		{"percent_encode",HttpPercentEncode},
		{"percent_decode",HttpPercentDecode},
		{"query_encode",HttpQueryEncode},
		{"query_parse",HttpQueryParse},
		{"reason",HttpReason},
		{"date",HttpDate},
		{nullptr,nullptr}
	};
	lua_newtable(L);
	luaL_setfuncs(L,functions,0);
	lua_newtable(L);
	for(const auto& e : SimpleWeb::status_code_strings())
	{
		if(e.second.size() < 5)
			continue;
		lua_pushinteger(L,std::stoi(e.second.substr(0,3)));
		lua_setfield(L,-2,StatusName(e.second.substr(4)).c_str());
	}
	lua_setfield(L,-2,"status");
	lua_setfield(L,-2,"HTTP");
}

const luaL_Reg ServerMethods[] =
{
	{"route",[](lua_State* L) -> int { return Self(L).LuaRoute(L); }},
	{"default_route",[](lua_State* L) -> int { return Self(L).LuaDefaultRoute(L); }},
	{"start",[](lua_State* L) -> int { return Self(L).LuaStart(L); }},
	{"stop",[](lua_State* L) -> int { return Self(L).LuaStop(L); }},
	{"port",[](lua_State* L) -> int { return Self(L).LuaPort(L); }},
	{nullptr,nullptr}
};

const luaL_Reg ResponseMethods[] =
{
	{"write",ResWrite},
	{"close_after",ResCloseAfter},
	{nullptr,nullptr}
};
} //namespace

void ExportHttpWrappers(lua_State* const L,
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
	if(luaL_newmetatable(L,ServerMeta))
	{
		lua_pushcfunction(L,ServerGC);
		lua_setfield(L,-2,"__gc");
		lua_newtable(L);
		luaL_setfuncs(L,ServerMethods,0);
		lua_setfield(L,-2,"__index");
	}
	lua_pop(L,1);
	if(luaL_newmetatable(L,ResponseMeta))
	{
		lua_pushcfunction(L,ResponseGC);
		lua_setfield(L,-2,"__gc");
		lua_newtable(L);
		luaL_setfuncs(L,ResponseMethods,0);
		lua_setfield(L,-2,"__index");
	}
	lua_pop(L,1);

	auto ctx = std::make_shared<HttpServerObj::Context>();
	ctx->port = PortInfo{L,pSyncStrand,handler_tracker,Name,LogName};
	auto p = lua_newuserdatauv(L,sizeof(std::shared_ptr<HttpServerObj::Context>),0);
	new(p) std::shared_ptr<HttpServerObj::Context>(ctx);
	luaL_setmetatable(L,CtxMeta);
	lua_pushvalue(L,-1);
	lua_setfield(L,LUA_REGISTRYINDEX,CtxKey);
	lua_pushcclosure(L,ServerNew,1);
	lua_setfield(L,-2,"HTTPServer");

	if(luaL_newmetatable(L,ClientCtxMeta))
	{
		lua_pushcfunction(L,ClientCtxGC);
		lua_setfield(L,-2,"__gc");
	}
	lua_pop(L,1);
	if(luaL_newmetatable(L,ClientMeta))
	{
		lua_pushcfunction(L,ClientGC);
		lua_setfield(L,-2,"__gc");
		lua_newtable(L);
		luaL_setfuncs(L,ClientMethods,0);
		lua_setfield(L,-2,"__index");
	}
	lua_pop(L,1);
	auto client_ctx = std::make_shared<HttpClientObj::Context>();
	client_ctx->port = PortInfo{L,pSyncStrand,handler_tracker,Name,LogName};
	auto cp = lua_newuserdatauv(L,sizeof(std::shared_ptr<HttpClientObj::Context>),0);
	new(cp) std::shared_ptr<HttpClientObj::Context>(client_ctx);
	luaL_setmetatable(L,ClientCtxMeta);
	lua_pushvalue(L,-1);
	lua_setfield(L,LUA_REGISTRYINDEX,ClientCtxKey);
	lua_pushcclosure(L,ClientNew,1);
	lua_setfield(L,-2,"HTTPClient");
	ExportHttpHelpers(L);
	lua_pop(L,1);
}

void CloseHttpObjects(lua_State* const L, bool permanent)
{
	lua_getfield(L,LUA_REGISTRYINDEX,ClientCtxKey);
	auto cp = static_cast<std::shared_ptr<HttpClientObj::Context>*>(lua_touserdata(L,-1));
	std::shared_ptr<HttpClientObj::Context> client_ctx = cp ? *cp : nullptr;
	lua_pop(L,1);
	if(client_ctx)
	{
		if(permanent)
			client_ctx->dead = true;
		const auto live = client_ctx->live;
		for(const auto& o : live)
			o->CloseForPort();
	}

	lua_getfield(L,LUA_REGISTRYINDEX,CtxKey);
	auto p = static_cast<std::shared_ptr<HttpServerObj::Context>*>(lua_touserdata(L,-1));
	std::shared_ptr<HttpServerObj::Context> ctx = p ? *p : nullptr;
	lua_pop(L,1);
	if(!ctx)
		return;
	if(permanent)
		ctx->dead = true;
	const auto live = ctx->live;
	for(const auto& o : live)
		o->Stop(true);
}

size_t HttpObjectCount()
{
	return LiveServers.load()+LiveClients.load();
}
