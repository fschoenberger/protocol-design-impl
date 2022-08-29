#include "pch.hpp"
#include "Logger.hpp"

#include <boost/log/utility/setup/formatter_parser.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/expressions/formatters/stream.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/expressions/formatters/auto_newline.hpp>
#include <boost/log/expressions/formatters/if.hpp>
#include <boost/log/expressions/predicates/is_in_range.hpp>
#include <boost/log/expressions/formatters/wrap_formatter.hpp>
#include <boost/log/attributes/attribute.hpp>
#include <boost/log/attributes/attribute_value.hpp>
#include <boost/log/attributes/attribute_value_impl.hpp>
#include <boost/log/keywords/facility.hpp>
#include <boost/log/keywords/use_impl.hpp>
#include <boost/log/sinks.hpp>
#include <boost/format.hpp>
#include <boost/phoenix.hpp>

// Supporting headers
#include <codecvt>
#include <boost/log/support/date_time.hpp>

#include <iostream>

namespace expr = boost::log::expressions;
namespace attr = boost::log::attributes;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

namespace rft {

namespace {
enum Color {
    kBlue,
    kCyan,
    kFaint,
    kGreen,
    kMagenta,
    kRed,
    kYellow,
};

auto coloringExpression(auto fmt, Color color) {
    return expr::wrap_formatter([fmt, color](boost::log::record_view const& rec, boost::log::formatting_ostream& stream) {
        const static auto lookup = std::array{
            "\033[34m",
            "\033[36m",
            "\033[2;90m",
            "\033[32m",
            "\033[35m",
            "\033[31m",
            "\033[33m",
        };
        static_assert(lookup.size() - 1 == kYellow);

        stream << lookup[color];
        fmt(rec, stream);
        stream << "\033[0;10m";
    });
}
}

gLogger::logger_type gLogger::construct_logger() {
    auto core = boost::log::core::get();

    boost::log::sources::severity_channel_logger_mt<Severity> lg;
    boost::log::add_common_attributes();
    boost::log::register_simple_formatter_factory<Severity, char>("Severity");
    //core->add_global_attribute("ThreadName", ThreadNameAttribute());

    auto sink = boost::log::add_console_log(std::cout);
    sink->set_formatter(expr::stream
                        << coloringExpression( // %clr(%d{${LOG_DATEFORMAT_PATTERN:-yyyy-MM-dd'T'HH:mm:ss.SSSXXX}}){faint}
                            expr::stream << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f"),
                            kFaint)
                        << "  "
                        << std::setw(5)
                        << expr::if_(is_in_range(expr::attr<Severity>("Severity"), kTrace, kWarning))[
                            expr::stream << coloringExpression(expr::stream << expr::attr<Severity>("Severity"), kGreen) // %clr(${LOG_LEVEL_PATTERN:-%5p})
                        ].else_[
                            expr::stream << expr::if_(
                                is_in_range(expr::attr<Severity>("Severity"), kWarning, kError))[
                                expr::stream << coloringExpression(expr::stream << expr::attr<Severity>("Severity"), kYellow)
                                // %clr(${LOG_LEVEL_PATTERN:-%5p})
                            ].else_[
                                expr::stream << coloringExpression(expr::stream << expr::attr<Severity>("Severity"), kRed)
                            ]
                        ]
                        << " "
                        << coloringExpression(expr::stream << expr::attr<attr::current_process_id::value_type>("ProcessID"), kMagenta) // %clr(${PID:- }){magenta}
                        << coloringExpression(expr::stream << " --- ", kFaint)                                                         // %clr(---){faint}
                        << coloringExpression(expr::stream << "[" << expr::attr<attr::current_thread_id::value_type>("ThreadID") << "]",
                                              kFaint) // %clr([%15.15t]){faint}
                        //<< coloringExpression(expr::attr<std::string>("Channel"), kCyan) // %clr(%-40.40logger{39}){cyan}
                        << coloringExpression(expr::stream << " : ", kFaint) // %clr(:){faint}
                        << expr::smessage                                    //%m
                        << expr::auto_newline                                //%n
        );

    core->add_sink(sink);

    return lg;
}

} // namespace prcd::log
