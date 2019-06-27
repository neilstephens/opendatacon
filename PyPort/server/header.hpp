//
// header.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_HEADER_HPP
#define HTTP_HEADER_HPP

#include <string>
#include "spdlog/spdlog.h"

// Hide some of the code to make Logging cleaner
#define LOGDEBUG(...) \
	if (auto log = spdlog::get("PyPort")) \
		log->debug(__VA_ARGS__);
#define LOGERROR(...) \
	if (auto log = spdlog::get("PyPort")) \
		log->error(__VA_ARGS__);
#define LOGWARN(...) \
	if (auto log = spdlog::get("PyPort"))  \
		log->warn(__VA_ARGS__);
#define LOGINFO(...) \
	if (auto log = spdlog::get("PyPort")) \
		log->info(__VA_ARGS__);

namespace http
{
		struct header
		{
			std::string name;
			std::string value;
		};
} // namespace http

#endif // HTTP_HEADER_HPP
