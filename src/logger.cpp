#include "stdafx.h"
#include "logger.h"

#include <boost/bind/bind.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/trivial.hpp>
//#include <boost/log/attributes.hpp>
#include <boost/log/attributes/function.hpp>

void NoOp(const std::string& msg) {
}

void StdOut(const std::string& msg, const std::string& level) {
    std::cout << Logger::Timestamp() << ": [" << level << "] " << msg << std::endl;
}

std::function<void(const std::string&)> Logger::trace_ = NoOp;
std::function<void(const std::string&)> Logger::debug_ = NoOp;
std::function<void(const std::string&)> Logger::info_ = boost::bind<void, const std::string&, const std::string&>(&StdOut, boost::placeholders::_1, "info");
std::function<void(const std::string&)> Logger::warning_ = boost::bind<void, const std::string&, const std::string&>(&StdOut, boost::placeholders::_1, "warning");
std::function<void(const std::string&)> Logger::error_ = boost::bind<void, const std::string&, const std::string&>(&StdOut, boost::placeholders::_1, "error");
std::function<void(const std::string&)> Logger::fatal_ = boost::bind<void, const std::string&, const std::string&>(&StdOut, boost::placeholders::_1, "fatal");

std::string Logger::Timestamp() {
    const boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
    const boost::posix_time::ptime utc = boost::posix_time::second_clock::universal_time();
    const boost::posix_time::ptime local = boost::date_time::c_local_adjustor<boost::posix_time::ptime>::utc_to_local(utc);
    const boost::posix_time::posix_time_system::time_duration_type td = local - utc;
    std::ostringstream s;
    s << boost::posix_time::to_iso_extended_string(now);
    s.seekp(23); // millisec
    s << (td.is_negative() ? '-' : '+');
    s << std::setw(2) << std::setfill('0') << boost::date_time::absolute_value(td.hours()) << ':';
    s << std::setw(2) << std::setfill('0') << boost::date_time::absolute_value(td.minutes());
    return s.str();
}

void Logger::Init(const Json::Node& conf, const std::string& name) {
    boost::log::core::get()->add_global_attribute("TimeStamp", boost::log::attributes::make_function(&Logger::Timestamp));
    boost::log::add_console_log(
        std::cout,
        boost::log::keywords::auto_flush = true,
        boost::log::keywords::format = boost::log::expressions::format("%1%: [%2%] %3%")
        % boost::log::expressions::attr<std::string>("TimeStamp")
        % boost::log::trivial::severity
        % boost::log::expressions::message
    );
    boost::filesystem::path target = conf["target"].to<boost::filesystem::path>();
    if (!target.empty()) {
        auto file_sink = boost::log::add_file_log(
            boost::log::keywords::auto_flush = true,
            boost::log::keywords::open_mode = std::ios::app,
            boost::log::keywords::target = target,
            boost::log::keywords::file_name = target / (name + "_%Y%m%d.log"),
            //boost::log::keywords::file_name = target / (name + "_%Y%m%d_%N.log"),
            //boost::log::keywords::rotation_size = conf["rotation_size"].to<size_t>(10 * 1024 * 1024),
            boost::log::keywords::max_size = conf["max_size"].to<size_t>(1024 * 1024 * 1024),
            boost::log::keywords::max_files = conf["max_files"].to<size_t>(30),
            boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(0, 0, 0),
            boost::log::keywords::format = boost::log::expressions::format("%1%: [%2%] %3%")
            % boost::log::expressions::attr<std::string>("TimeStamp")
            % boost::log::trivial::severity
            % boost::log::expressions::message
        );
        //std::locale loc("");
        //file_sink->imbue(loc);
    }
    boost::log::add_common_attributes();
    std::string level = conf["level"].to<std::string>();
    if (boost::istarts_with(level, "t")) {
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::trace);
    } else if (boost::istarts_with(level, "d")) {
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::debug);
    } else if (boost::istarts_with(level, "w")) {
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::warning);
    } else if (boost::istarts_with(level, "e")) {
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::error);
    } else if (boost::istarts_with(level, "f")) {
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::fatal);
    } else {
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
    }
    trace_ = [](const std::string& msg) -> void { BOOST_LOG_TRIVIAL(trace) << msg; };
    debug_ = [](const std::string& msg) -> void { BOOST_LOG_TRIVIAL(debug) << msg; };
    info_ = [](const std::string& msg) -> void { BOOST_LOG_TRIVIAL(info) << msg; };
    warning_ = [](const std::string& msg) -> void { BOOST_LOG_TRIVIAL(warning) << msg; };
    error_ = [](const std::string& msg) -> void { BOOST_LOG_TRIVIAL(error) << msg; };
    fatal_ = [](const std::string& msg) -> void { BOOST_LOG_TRIVIAL(fatal) << msg; };
}

void Logger::Term() {
    boost::log::core::get()->remove_all_sinks();
    trace_ = NoOp;
    debug_ = NoOp;
    info_ = boost::bind<void, const std::string&, const std::string&>(&StdOut, boost::placeholders::_1, "info");
    warning_ = boost::bind<void, const std::string&, const std::string&>(&StdOut, boost::placeholders::_1, "warning");
    error_ = boost::bind<void, const std::string&, const std::string&>(&StdOut, boost::placeholders::_1, "error");
    fatal_ = boost::bind<void, const std::string&, const std::string&>(&StdOut, boost::placeholders::_1, "fatal");
}
