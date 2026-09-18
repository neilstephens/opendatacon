//tiny wrapper to supress warnings from spdlog (fmt) by 'marking' it as system header
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC system_header
#endif

#ifndef ODC_SPDLOG_WRAPPER_H
#define ODC_SPDLOG_WRAPPER_H

#include <spdlog/spdlog.h>
#include <spdlog/fmt/std.h>

#endif // ODC_SPDLOG_WRAPPER_H
