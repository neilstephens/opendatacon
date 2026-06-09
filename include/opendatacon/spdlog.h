//tiny wrapper to supress warnings from spdlog (fmt) by 'marking' it as system header
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC system_header
#endif
#include <spdlog/spdlog.h>
#include <spdlog/fmt/std.h>
