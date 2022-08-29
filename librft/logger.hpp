#pragma once

#include <format>

//#define BOOST_USE_WINAPI_VERSION BOOST_WINAPI_VERSION_WIN7

#include <boost/log/common.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>

namespace rft {
enum Severity {
    kTrace,
    kDebug,
    kInfo,
    kWarning,
    kError,
    kFatal
};

constexpr static auto SEVERITY_LEVEL_STR = std::array{"TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"};

template <typename CharT, typename TraitsT>
constexpr std::basic_ostream<CharT, TraitsT>& operator<<(std::basic_ostream<CharT, TraitsT>& stream, rft::Severity level) {
    const char* str = SEVERITY_LEVEL_STR[level];
    if (level < SEVERITY_LEVEL_STR.size() && level >= 0)
        stream << str;
    else
        stream << static_cast<int>(level);
    return stream;
}

BOOST_LOG_GLOBAL_LOGGER(gLogger, boost::log::sources::severity_channel_logger_mt<Severity>)
} // namespace prcd::log

#define LOG_FATAL(...) BOOST_LOG_SEV(rft::gLogger::get(), rft::Severity::kFatal) << std::format(##__VA_ARGS__)
#define LOG_ERROR(...) BOOST_LOG_SEV(rft::gLogger::get(), rft::Severity::kError) << std::format(##__VA_ARGS__)
#define LOG_WARNING(...) BOOST_LOG_SEV(rft::gLogger::get(), rft::Severity::kWarning) << std::format(##__VA_ARGS__)
#define LOG_INFO(...) BOOST_LOG_SEV(rft::gLogger::get(), rft::Severity::kInfo) << std::format(##__VA_ARGS__)
#define LOG_DEBUG(...) BOOST_LOG_SEV(rft::gLogger::get(), rft::Severity::kDebug) << std::format(##__VA_ARGS__)
#define LOG_TRACE(...) BOOST_LOG_SEV(rft::gLogger::get(), rft::Severity::kTrace) << std::format(##__VA_ARGS__)
