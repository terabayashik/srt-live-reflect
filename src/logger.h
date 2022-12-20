#pragma once

#include "json.h"

class Logger
{
    static std::function<void(const std::string&)> trace_;
    static std::function<void(const std::string&)> debug_;
    static std::function<void(const std::string&)> info_;
    static std::function<void(const std::string&)> warning_;
    static std::function<void(const std::string&)> error_;
    static std::function<void(const std::string&)> fatal_;
    static int level_;
public:
    static std::string Timestamp();
    static void Init(const Json::Node& conf, const std::string& name);
    static void Term();
    static bool TraceEnabled();
    static void Trace(const std::string& msg) { trace_(msg); }
    static void Trace(const boost::format& fmt) { trace_(fmt.str()); }
    static bool DebugEnabled();
    static void Debug(const std::string& msg) { debug_(msg); }
    static void Debug(const boost::format& fmt) { debug_(fmt.str()); }
    static bool InfoEnabled();
    static void Info(const std::string& msg) { info_(msg); }
    static void Info(const boost::format& fmt) { info_(fmt.str()); }
    static bool WarningEnabled();
    static void Warning(const std::string& msg) { warning_(msg); }
    static void Warning(const boost::format& fmt) { warning_(fmt.str()); }
    static bool ErrorEnabled();
    static void Error(const std::string& msg) { error_(msg); }
    static void Error(const boost::format& fmt) { error_(fmt.str()); }
    static bool FatalEnabled();
    static void Fatal(const std::string& msg) { fatal_(msg); }
    static void Fatal(const boost::format& fmt) { fatal_(fmt.str()); }
};
