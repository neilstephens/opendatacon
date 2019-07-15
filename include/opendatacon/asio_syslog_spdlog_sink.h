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
#ifndef ASIO_SYSLOG_SPDLOG_SINK_H
#define ASIO_SYSLOG_SPDLOG_SINK_H

#include <opendatacon/asio.h>
#include <spdlog/sinks/sink.h>
#include <sstream>
#include <iomanip>

namespace odc
{
class asio_syslog_spdlog_sink: public spdlog::sinks::sink
{
public:
	asio_syslog_spdlog_sink(
		odc::asio_service& ios,
		const std::string& dst_host,
		const std::string& dst_port,
		const int facility = 1,
		const std::string& local_host = "-",
		const std::string& app = "-",
		const std::string& category = "-"):
		resolver(ios.make_udp_resolver()),
		query(asio::ip::udp::v4(), dst_host, dst_port),
		endpoint(*resolver->resolve(query)),
		socket(ios.make_udp_socket()),
		facility_(facility)
	{
		socket->open(asio::ip::udp::v4());
		//syslog header - looks like "<8*Facility+Severity>VERSION YYYY-MM-DDThh:mm:ss.sss+/-hh:mm HOSTNAME APP-NAME PROCID MSGID (BOM?)MSG"
		//use formatter pattern to do everything except <8*Facility+Severity>
		std::string pattern = "1 %Y-%m-%dT%T.%e%z " + local_host + " " + app + " %P " + category +
		                      " [%n] [%l] %v";
		formatter_ = spdlog::details::make_unique<spdlog::pattern_formatter>(pattern);

		severities[static_cast<size_t>(spdlog::level::trace)] = 7;
		severities[static_cast<size_t>(spdlog::level::debug)] = 7;
		severities[static_cast<size_t>(spdlog::level::info)] = 6;
		severities[static_cast<size_t>(spdlog::level::warn)] = 4;
		severities[static_cast<size_t>(spdlog::level::err)] = 3;
		severities[static_cast<size_t>(spdlog::level::critical)] = 2;
		severities[static_cast<size_t>(spdlog::level::off)] = 6;
	}

	void log(const spdlog::details::log_msg &msg) override
	{
		//build full syslog datagram in here
		//TODO: probably a more efficient way using formatter append instead of ss
		std::stringstream msg_ss;
		msg_ss << "<" << facility_*8+severities[static_cast<size_t>(msg.level)] << ">";

		fmt::memory_buffer formatted;
		formatter_->format(msg, formatted);
		msg_ss.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));

		socket->send_to(asio::buffer(msg_ss.str().c_str(),msg_ss.str().size()), endpoint);
	}


	void flush() override {}
	void set_pattern(const std::string &pattern) override {}
	void set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) override {}

private:
	std::unique_ptr<asio::ip::udp::resolver> resolver;
	asio::ip::udp::resolver::query query;
	asio::ip::udp::endpoint endpoint;
	std::unique_ptr<asio::ip::udp::socket> socket;
	std::array<int, 7> severities;
	int facility_;
};

} // namespace odc

#endif // ASIO_SYSLOG_SPDLOG_SINK_H
