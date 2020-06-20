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
//
//  WebUI.cpp
//  opendatacon
//
//  Created by Alan Murray on 06/09/2014.
//
//

#include "WebUI.h"
#include <opendatacon/util.h>
#include <opendatacon/asio.h>

/* Test Certificate */
//openssl genrsa -out server.key 2048
const char default_cert_pem[] = "\
-----BEGIN CERTIFICATE-----\
MIIDTzCCAjegAwIBAgIJALMlRzO1GxWWMA0GCSqGSIb3DQEBBQUAMCMxITAfBgNV\
BAoTGEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDAeFw0xNDA4MzEwNzEwMDdaFw0y\
NDA4MjgwNzEwMDdaMCMxITAfBgNVBAoTGEludGVybmV0IFdpZGdpdHMgUHR5IEx0\
ZDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALy6snqeSt9zLRbuxJMC\
nIIuvV9MLBWf6G4f1yr51tvNrL63Z+QtVn4n+EclSZMSzbYwWGDudWQEI3/aB6TW\
45gXiZiINwCuhRWCIMhfRjar0pCwuinA/m+oK4n/hMcR/CH2kocUIB1XWRZojRXz\
UPJvgeN41vmbzskRx/NiiSW+L0LeaXsO9lNVid+TQqLuoEC3UuDiF9wgxaB8bwxB\
tzHkzY+ZiH1JhPLCKy7vmMNdZ0IBd7ZJWS8R3v5PJKOtsiAeT6gscQajpBl3a05w\
A5F6A7tguLpwEbds19RI7AhTvceJdKzCBbJD6gQLVRkwOlxdCcNo+lIcLi/mWfro\
GeECAwEAAaOBhTCBgjAdBgNVHQ4EFgQUPMEnSKiWMmJSPuXLK+J+hRUBVhgwUwYD\
VR0jBEwwSoAUPMEnSKiWMmJSPuXLK+J+hRUBVhihJ6QlMCMxITAfBgNVBAoTGElu\
dGVybmV0IFdpZGdpdHMgUHR5IEx0ZIIJALMlRzO1GxWWMAwGA1UdEwQFMAMBAf8w\
DQYJKoZIhvcNAQEFBQADggEBAKMrwZx/VuUu2jrsCy5SSi4Z2U3QCo08TtmJXYY+\
ekZGAQud84bfJTcr3uR0ubLZDHhxU35N96k8YCkJERx4MUhnwuJHa7nEhJcrsM5z\
ZSKcZ5wpH6JDDt1DN4Hms+PMiLuDkPfZL7dV1r9GFrzN1/PYKrD1K5QQyt9I/MAD\
WBP6nipRqM2kEscggH911XntElBUnj9jFjFnpHjaNJAnz05PAORXrCrXA2EKz6RH\
y/Ep3/khCkj2C3DlRowRTzQwJ0eezMf7UsjeRkZZIvjis1381owJrRm3yjRYDUa6\
7e03d+42UKqZx1Ka1to5D6Al1ygiP3hl0bQj+/wpToK6uVw=\
-----END CERTIFICATE-----";

//openssl req -days 3650 -out server.pem -new -x509 -key server.key
const char default_key_pem[] = "\
-----BEGIN RSA PRIVATE KEY-----\
MIIEpAIBAAKCAQEAvLqyep5K33MtFu7EkwKcgi69X0wsFZ/obh/XKvnW282svrdn\
5C1Wfif4RyVJkxLNtjBYYO51ZAQjf9oHpNbjmBeJmIg3AK6FFYIgyF9GNqvSkLC6\
KcD+b6grif+ExxH8IfaShxQgHVdZFmiNFfNQ8m+B43jW+ZvOyRHH82KJJb4vQt5p\
ew72U1WJ35NCou6gQLdS4OIX3CDFoHxvDEG3MeTNj5mIfUmE8sIrLu+Yw11nQgF3\
tklZLxHe/k8ko62yIB5PqCxxBqOkGXdrTnADkXoDu2C4unARt2zX1EjsCFO9x4l0\
rMIFskPqBAtVGTA6XF0Jw2j6UhwuL+ZZ+ugZ4QIDAQABAoIBAQCxkIYzz5JqQZb+\
qI7SMfbGlOsfKi+f+N9aHSL4EDAShaQtm6lniTCDaV+ysGZUtbBN5ZaBPFm+TBaK\
R7xBXtyrUBnpJN97CLe10MS/QMRy0548+8lrV2UL8JFmOL3X/hfWbILYDBta/7+V\
0bBMIqzaLAds2ViJaApaKxyQ5PhcRMFxLPnm7SRdltScpjkGQNcC2ilA+ezknOq1\
rj0MR0niaMezwsz590/h9qUAkxBxJSZL86HOKiZ678haNwgHrgxQBJPIwTliEB9M\
xPTxLyM+feHu9oUpYgzrEV/+sBENZY3nsqj8iinIYFCZGcRUAnyjRKDNZn3/tYmN\
xP8KXLExAoGBAOX+P3+w8lZoMdvtPu4NYiCmNmJTVa2raqgq02drswZDKf1LOJoW\
GSXUr9xkpg5BqQyRys+yJwFXKTwvH2Py/KrBUuKj/UusKmM/ycnj/an5w7zmeubB\
bUahUTmLiDM5jzvv4gDqfoGswZoxhJ9XGWpVCOFdbpukMzHe3MiI0xKbAoGBANIR\
9XDKMuXbUOiJ+X+pSXuvfYSklgoKaFzr6ozsN+jXiPdw9WWF+j2yz8zb0v6fkwJi\
HlHIuvosnNf6L2UGF/5T0Eal7yIEsPK+MTgIw0Zr0IOpCUasAbALkcQMd+rBjdbV\
NeOpVwC/Zr1kC5hKI+V6VA7QmWLjb+Fy8E4jqf8zAoGBANbLSlZg1RKpoNb6jSkZ\
yqkfUe8mUQAu9R81T9ZomPuiQlbSp3wQY1AXgF5eiU8LN2wLxNOQWClCU7pnb/OS\
fTKj9lrAONExayzh5/zrNn5GSu3ieqmDwCCUjB0oGP1uJj0d3X5pgdhtlSoCUQ/W\
8l+CJxcCgUhOY5mRv7RxRF89AoGAdL7yTrqwyrm2H2X+uQoWAp0m/r6RfAcItQuP\
kL3+3HJcdlfaqY9p4Tws7EcG3edFRj/NZdpOv5ZnnEg4asaWMwvVZk31tkwxIta8\
d8226L4mZeVdeF9DmNj1K6VaR6dF8q0Pg/Sqm4nDyWF+aCZcCL6RVKJtfF214e+E\
yYhcg60CgYATTs3kW5cmGfdvkXHCSodIHonZLzHLOkn81S0W6FEM8zG1MSVLPA+J\
DE+cahwFk7x5dZ28WePVnm/QqIFdyq3g9MliQrlIGVbbn3DtvVVBT5cc2/NPDnHN\
9Ew4HhHIV+smoLTlGglfrlCuHXcrEzK5l5AMy9gD62OnhR3b3y0o4g==\
-----END RSA PRIVATE KEY-----";

/* response handler callback wrapper */
static int ahc(void *cls,
	struct MHD_Connection *connection,
	const char *url,
	const char *method,
	const char *version,
	const char *upload_data,
	size_t *upload_data_size,
	void **ptr)
{
	auto test = reinterpret_cast<WebUI*>(cls);
	std::string upload_data_str;
	if (*upload_data_size > 0)
	{
		upload_data_str = upload_data;
	}
	return test->http_ahc(cls, connection, url, method, version, upload_data_str, *upload_data_size, ptr);
}

static
std::string load_key(const char *filename)
{
	std::ifstream in;
	in.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	in.open(filename, std::ios::in | std::ios::binary);
	std::string contents;
	in.seekg(0, std::ios::end);
	contents.resize(in.tellg());
	in.seekg(0, std::ios::beg);
	in.read(&contents[0], contents.size());
	in.close();
	return(contents);
}

WebUI::WebUI(uint16_t pPort, const std::string& web_root, const std::string& tcp_port):
	d(nullptr),
	port(pPort),
	web_root(web_root),
	tcp_port(tcp_port),
	pSockMan(nullptr),
	pLogRegex(nullptr)
{
	try
	{
		key_pem = load_key("server.key");
		cert_pem = load_key("server.pem");
	}
	catch (std::exception& e)
	{
		if(auto log = odc::spdlog_get("WebUI"))
			log->warn("WebUI port {}: The key/certificate files could not be read. Reverting to default certificate.", pPort);
		cert_pem = default_cert_pem;
		key_pem = default_key_pem;
	}
}

void WebUI::AddCommand(const std::string& name, std::function<void (std::stringstream&)> callback, const std::string& desc)
{
	RootCommands[name] = callback;
}

void WebUI::AddResponder(const std::string& name, const IUIResponder& pResponder)
{
	Responders["/" + name] = &pResponder;
}

/* HTTP access handler call back */
int WebUI::http_ahc(void *cls,
	struct MHD_Connection *connection,
	const std::string& url,
	const std::string& method,
	const std::string& version,
	const std::string& upload_data,
	size_t& upload_data_size,
	void **con_cls)
{
	struct connection_info_struct *con_info;

	//
	if(nullptr == *con_cls)
	{
		return CreateNewRequest(cls,
			connection,
			url,
			method,
			version,
			upload_data,
			upload_data_size,
			con_cls);
	}

	ParamCollection params;

	if (method == "POST")
	{
		con_info = reinterpret_cast<connection_info_struct*>(*con_cls);

		if (upload_data_size > 0)
		{
			MHD_post_process(con_info->postprocessor, upload_data.c_str(), upload_data_size);
			upload_data_size = 0;

			return MHD_YES;
		}
		if (con_info)
		{
			params = con_info->PostValues;
		}
	}

	/* it will handle the OpenDataCon requests */
	if (url.find("/OpenDataCon") != std::string::npos)
	{
		const std::string data = HandleOpenDataCon(url);
		return ReturnJSON(connection, data);
	}

	/* it will handle the SimControl requests */
	if (url.find("/SimControl") != std::string::npos)
	{
		const std::string data = HandleSimControl(url);
		return ReturnJSON(connection, data);
	}

	const std::string ResponderName = GetPath(url);
	if (Responders.count(ResponderName))
	{
		const std::string command = GetFile(&url[ResponderName.length()]);
		Json::Value event;

		//TODO: make this writer reusable (class member)
		Json::StreamWriterBuilder wbuilder;
		wbuilder["commentStyle"] = "None";
		std::unique_ptr<Json::StreamWriter> const pWriter(wbuilder.newStreamWriter());
		std::ostringstream oss;

		event = Responders[ResponderName]->ExecuteCommand(command, params);
		pWriter->write(event, &oss); oss<<std::endl;

		std::string data = oss.str();
		/* Remove the list command for DataPorts  */
		if ((url == "/DataPorts/List" ||
		     url == "/DataConnectors/List") && method == "POST")
		{
			const std::size_t pos = data.find("List");
			std::string temp = data.substr(0, pos - 2);
			// we need to skip "List,"\n therefore we have a constant as 8
			temp += data.substr(pos + 8, data.size() - pos + 8);
			data = temp;
		}

		return ReturnJSON(connection, data);
	}
	else
	{
		if (url == "/")
		{
			return ReturnFile(connection, web_root + "/" + ROOTPAGE);
		}
		else
		{
			return ReturnFile(connection, web_root + "/" + url);
		}
	}
}

void WebUI::Build()
{
	std::string cmd = "tcp_web_ui off TCP localhost " + tcp_port + " SERVER";
	std::stringstream ss(cmd);
	RootCommands["add_logsink"](ss);
}

void WebUI::Enable()
{
	if (useSSL)
	{
		d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_DEBUG | MHD_USE_SSL,
			port,                                                  // Port to bind to
			nullptr,                                               // callback to call to check which clients allowed to connect
			nullptr,                                               // extra argument to apc
			&ahc,                                                  // handler called for all requests
			this,                                                  // extra argument to dh
			MHD_OPTION_NOTIFY_COMPLETED, &request_completed, this, // completed handler and extra argument
			MHD_OPTION_CONNECTION_TIMEOUT, 256,
			MHD_OPTION_HTTPS_MEM_KEY, key_pem.c_str(),
			MHD_OPTION_HTTPS_MEM_CERT, cert_pem.c_str(),
			MHD_OPTION_END);
	}
	else
	{
		d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_DEBUG,
			port,                                                  // Port to bind to
			nullptr,                                               // callback to call to check which clients allowed to connect
			nullptr,                                               // extra argument to apc
			&ahc,                                                  // handler called for all requests
			this,                                                  // extra argument to dh
			MHD_OPTION_NOTIFY_COMPLETED, &request_completed, this, // completed handler and extra argument
			MHD_OPTION_CONNECTION_TIMEOUT, 256,
			MHD_OPTION_END);
	}

	if (d == nullptr)
	{
		// TODO: log error message
	}
}

void WebUI::Disable()
{
	if (d == nullptr) return;
	MHD_stop_daemon(d);
	d = nullptr;
	pSockMan->Close();
}

std::string WebUI::HandleSimControl(const std::string& url)
{
	const std::string simcontrol = "/SimControl";
	const std::string command = url.substr(simcontrol.size() + 1, url.size() - simcontrol.size());

	Json::Value value;
	std::string data;
	ParamCollection params;

	if (command != "List")
	{
		std::string ports, type, index, value;
		std::istringstream iss(command);
		iss >> command;
		iss >> ports;
		iss >> type;
		iss >> index;
		params["Target"] = ports;
		params["0"] = type;
		params["1"] = index;

		if (command != "ReleasePoint")
		{
			iss >> value;
			params["2"] = value;
		}
	}

	value = Responders[simcontrol]->ExecuteCommand(command, params);
	Json::StreamWriterBuilder wbuilder;
	std::unique_ptr<Json::StreamWriter> const pWriter(wbuilder.newStreamWriter());
	std::ostringstream oss;
	pWriter->write(value, &oss); oss<<std::endl;
	return oss.str();
}

std::string WebUI::HandleOpenDataCon(const std::string& url)
{
	const std::string opendatacon = "/OpenDataCon";
	const std::string set_log_level = "set_loglevel";
	const std::string command = url.substr(opendatacon.size() + 1, url.size() - opendatacon.size());

	Json::Value value;
	if (command == "List")
	{
		for (auto it = RootCommands.begin();
		     it != RootCommands.end(); ++it)
		{
			value[it->first] = it->first;
		}
	}
	else if (command == "version")
	{
		ParamCollection params;
		value = Responders[opendatacon]->ExecuteCommand(command, params);
	}
	else if (command.find(set_log_level) != std::string::npos)
	{
		const std::string log_type = command.substr(set_log_level.size(), command.size() - set_log_level.size());
		const std::string cmd = "tcp_web_ui " + log_type;
		std::stringstream ss(cmd);
		RootCommands["set_loglevel"](ss);
	}
	else if (command.find("tcp") != std::string::npos)
	{
		if (command == "tcp_logs_on")
		{
			if(!pSockMan)
				ConnectToTCPServer();

			std::atomic_bool executed(false);
			log_out_sync->post([this,&value,&executed]()
				{
					value["tcp_data"] = tcp_log_out.str();
					tcp_log_out.str("");
					tcp_log_out.clear();
					executed = true;
				});
			while(!executed)
				pIOS->poll_one();
		}
		else
		{
			const std::size_t pos = command.find(" ");
			if (pos != std::string::npos)
			{
				const std::string log_type = command.substr(pos, command.size() - pos);
				const std::string cmd = "tcp_web_ui" + log_type + " off";
				std::stringstream ss(cmd);
				RootCommands["set_loglevel"](ss);
			}
		}
	}
	else
	{
		std::stringstream ss;
		RootCommands[command](ss);
	}

	Json::StreamWriterBuilder wbuilder;
	std::unique_ptr<Json::StreamWriter> const pWriter(wbuilder.newStreamWriter());
	std::ostringstream oss;
	pWriter->write(value, &oss); oss<<std::endl;
	return oss.str();
}

void WebUI::ConnectToTCPServer()
{
	//Use the ODC TCP manager
	// Client connection to localhost on the port we set up for log sinking
	// Automatically retry to connect on error
	pSockMan = std::make_unique<odc::TCPSocketManager<std::string>>
		           (pIOS, false, "localhost", tcp_port,
		           [this](odc::buf_t& readbuf){ReadCompletionHandler(readbuf);},
		           [this](bool state){ConnectionEvent(state);},
		           1000,
		           true);
	pSockMan->Open(); //Auto re-open is enabled, so it's set and forget
}

void WebUI::ReadCompletionHandler(odc::buf_t& readbuf)
{
	std::istream iss(&readbuf);
	std::string message;
	while(std::getline(iss,message))
	{
		if(iss.eof()) //not a full line - put it back, and we're done
		{
			readbuf.sputn(message.c_str(),message.size());
			break;
		}

		log_out_sync->post([this,message{std::move(message)}]()
			{
				auto log_size = tcp_log_out.tellp();
				if(log_size > 1000000)
				{
				      if (auto log = odc::spdlog_get("WebUI"))
						log->error("Browser isn't fetching logs fast enough: Dumping {} bytes.", log_size);
				      tcp_log_out.str("");
				      tcp_log_out.clear();
				}
				if(!pLogRegex || std::regex_match(message,*pLogRegex))
					tcp_log_out << message << std::endl;
			});
	}
}

void WebUI::ConnectionEvent(bool state)
{
	if (auto log = odc::spdlog_get("WebUI"))
		log->debug("Log sink connection on port {} {}",tcp_port,state ? "opened" : "closed");
}

void WebUI::ApplyLogFilter(const std::string& regex_filter)
{
	try
	{
		std::regex regx(regex_filter,std::regex::extended);
		log_out_sync->post([=]()
			{
				pLogRegex = std::make_unique<std::regex>(regx);
			});
	}
	catch (std::exception& e)
	{
		if (auto log = odc::spdlog_get("WebUI"))
			log->error("Problem using '{}' as regex: {}",regex_filter,e.what());
	}
	return;
}
