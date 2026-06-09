/*	opendatacon
 *
 *	Copyright (c) 2018:
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
 * HttpServerManager.cpp
 *
 *  Created on: 2018-05-16
 *      Author: Scott Ellis - scott.ellis@novatex.com.au
 */

#include "HttpServerManager.h"
#include "Log.h"
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>

struct HttpServerManager::Impl
{
	HttpServer server;
	std::thread server_thread;
	std::atomic<bool> running{false};

	std::map<std::string, HandlerCallbackType> HandlerMap;
	std::mutex HandlerMutex;

	explicit Impl(const std::string& address, const std::string& port)
	{
		server.config.address = address;
		server.config.port    = static_cast<unsigned short>(std::stoul(port));

		auto dispatch_fn = [this](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request)
					 {
						 dispatch(response, request);
					 };
		server.resource["^/.*"]["GET"]  = dispatch_fn;
		server.resource["^/.*"]["POST"] = dispatch_fn;

		auto bad_request_fn = [](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> /*request*/)
					    {
						    response->write(SimpleWeb::StatusCode::client_error_bad_request, "Bad Request");
					    };
		server.default_resource["GET"]  = bad_request_fn;
		server.default_resource["POST"] = bad_request_fn;
	}

	void dispatch(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request)
	{
		// Lookup key: "METHOD /decoded-path" — SWS provides the decoded path directly
		std::string key = request->method + " " + request->path;

		HandlerCallbackType handler;
		{
			std::unique_lock<std::mutex> lck(HandlerMutex);
			std::size_t match_len = 0;
			for (auto& kv : HandlerMap)
			{
				if ((key.find(kv.first) == 0) && (kv.first.length() > match_len))
				{
					match_len = kv.first.length();
					handler   = kv.second;
				}
			}
		}

		if (!handler)
		{
			Log.Debug("HttpServerManager: no handler for {}", key);
			response->write(SimpleWeb::StatusCode::client_error_bad_request, "No matching handler");
			return;
		}

		handler(response, request);
	}

	void start()
	{
		bool expected = false;
		if (!running.compare_exchange_strong(expected, true))
			return;
		server_thread = std::thread([this]() { server.start(); });
	}

	void stop()
	{
		bool expected = true;
		if (!running.compare_exchange_strong(expected, false))
			return;
		server.stop();
		if (server_thread.joinable())
			server_thread.join();
	}

	~Impl() { stop(); }
};

std::unordered_map<std::string, std::weak_ptr<HttpServerManager>> HttpServerManager::ServerMap;
std::mutex HttpServerManager::ManagementMutex;

ServerTokenType::~ServerTokenType()
{}

HttpServerManager::HttpServerManager(std::shared_ptr<odc::asio_service> /*apIOS*/,
	const std::string& aEndPoint,
	const std::string& aPort):
	pImpl(std::make_unique<Impl>(aEndPoint, aPort)),
	EndPoint(aEndPoint),
	Port(aPort),
	InternalServerID(MakeServerID(aEndPoint, aPort))
{
	Log.Debug("Opened an HttpServerManager object {}", InternalServerID);
}

HttpServerManager::~HttpServerManager()
{}

ServerTokenType HttpServerManager::AddConnection(std::shared_ptr<odc::asio_service> apIOS,
	const std::string& aEndPoint,
	const std::string& aPort)
{
	std::unique_lock<std::mutex> lck(ManagementMutex);
	std::string ServerID = MakeServerID(aEndPoint, aPort);

	if (ServerMap.count(ServerID) != 0)
	{
		if (auto pSM = ServerMap[ServerID].lock())
		{
			Log.Debug("ServerTok already exists, using that connection - {}", ServerID);
			return ServerTokenType(ServerID, pSM);
		}
	}

	Log.Debug("First ServerTok for connection - {}", ServerID);
	auto pSM = std::make_shared<HttpServerManager>(apIOS, aEndPoint, aPort);
	ServerMap[ServerID] = pSM;
	return ServerTokenType(ServerID, pSM);
}

void HttpServerManager::StartConnection(const ServerTokenType& ServerTok)
{
	std::unique_lock<std::mutex> lck(ManagementMutex);
	if (auto pServerMgr = ServerTok.pServerManager)
		pServerMgr->pImpl->start();
	else
		Log.Error("Tried to start httpserver when the connection token was not valid");
}

void HttpServerManager::StopConnection(const ServerTokenType& ServerTok)
{
	std::unique_lock<std::mutex> lck(ManagementMutex);
	if (auto pServerMgr = ServerTok.pServerManager)
		pServerMgr->pImpl->stop();
	else
		Log.Error("Tried to stop httpserver when the connection token was not valid");
}

void HttpServerManager::AddHandler(const ServerTokenType& ServerTok,
	const std::string& urlpattern,
	HandlerCallbackType handler)
{
	std::unique_lock<std::mutex> lck(ManagementMutex);
	if (auto pServerMgr = ServerTok.pServerManager)
	{
		std::unique_lock<std::mutex> hlck(pServerMgr->pImpl->HandlerMutex);
		pServerMgr->pImpl->HandlerMap[urlpattern] = std::move(handler);
	}
	else
		Log.Error("Tried to add a urihandler when the httpserver was not valid");
}

size_t HttpServerManager::RemoveHandler(const ServerTokenType& ServerTok,
	const std::string& urlpattern)
{
	std::unique_lock<std::mutex> lck(ManagementMutex);
	if (auto pServerMgr = ServerTok.pServerManager)
	{
		std::unique_lock<std::mutex> hlck(pServerMgr->pImpl->HandlerMutex);
		pServerMgr->pImpl->HandlerMap.erase(urlpattern);
		return pServerMgr->pImpl->HandlerMap.size();
	}
	else
	{
		Log.Error("Tried to remove a urihandler when the httpserver was not valid");
		return 0;
	}
}
