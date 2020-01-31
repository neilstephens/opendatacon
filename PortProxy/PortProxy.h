#pragma once

#include <opendatacon/util.h>

// Hide some of the code to make Logging cleaner
#define LOGTRACE(...) \
	if (auto log = odc::spdlog_get("opendatacon")) \
		log->trace(__VA_ARGS__);
#define LOGDEBUG(...) \
	if (auto log = odc::spdlog_get("opendatacon")) \
		log->debug(__VA_ARGS__);
#define LOGERROR(...) \
	if (auto log = odc::spdlog_get("opendatacon")) \
		log->error(__VA_ARGS__);
#define LOGWARN(...) \
	if (auto log = odc::spdlog_get("opendatacon"))  \
		log->warn(__VA_ARGS__);
#define LOGINFO(...) \
	if (auto log = odc::spdlog_get("opendatacon")) \
		log->info(__VA_ARGS__);
