#pragma once
// Hide some of the code to make Logging cleaner
#define LOG(logger, filters, location, msg) \
	pLoggers->Log(openpal::LogEntry(logger, filters, location, std::string(msg).c_str(),-1));
