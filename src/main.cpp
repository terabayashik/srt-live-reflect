#include "stdafx.h"
#include "listener.h"
#include "receiver.h"
#include "sender.h"
#include "json.h"
#include "curl.h"

//----------------------------------------------------------------------------
/// @fn timestamp
//----------------------------------------------------------------------------
std::string timestamp() {
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

//----------------------------------------------------------------------------
/// @fn prefix
//----------------------------------------------------------------------------
std::string prefix(const std::string& app = "") {
    if (app.empty()) return timestamp() + ": ";
    return timestamp() + ": <" + app + "> ";
}

//----------------------------------------------------------------------------
/// @class ReflectSender
//----------------------------------------------------------------------------
class ReflectSender : public Event, private boost::noncopyable {
    Sender::ptr_t sender_;
    std::string app_;
    std::string name_;
    std::string peer_;
protected:
    ReflectSender(int sfd, const SendOption& option) : Event(), sender_(Sender::Create(sfd, option)) {
        app_ = option.Get<std::string>("app");
        name_ = option.Get<std::string>("name");
        peer_ = option.Get<std::string>("peer");
    }
public:
    static Event::ptr_t Create(int sfd, const SendOption& option) {
        return Event::ptr_t(new ReflectSender(sfd, option));
    }
    virtual ~ReflectSender() {
        sender_.reset();
    }
protected:
    bool OnReceive(const ReceiveOption& option, const Event::buf_t& buf, bool discrete) override {
        if (!sender_) return false;
        if (sender_->Send(buf)) return true;
        std::string err = sender_->GetErrMsg();
        sender_.reset();
        std::cout << prefix(app_) << "send failed [" << name_ << "] for " << peer_ << " : " << err << std::endl;
        return false;
    }
};

//----------------------------------------------------------------------------
/// @class Reflect
//----------------------------------------------------------------------------
class Reflect : public Event, public boost::enable_shared_from_this<Reflect>, private boost::noncopyable
{
    const Json conf_;
    Curl curl_;
    Listener::ptr_t listener_;
    Receiver::map_t receivers_;
    int32_t stats_;
    std::chrono::steady_clock::time_point stats_time_;
protected:
    Reflect(const Json& conf) : Event(), conf_(conf), curl_(), listener_(), receivers_(), stats_(0), stats_time_() {
    }
    Receiver::ptr_t FindReceiver(const std::string& name) const {
        const Receiver::map_t::const_iterator it = receivers_.find(name);
        return it == receivers_.end() ? Receiver::ptr_t() : it->second;
    }
public:
    typedef boost::shared_ptr<Reflect> ptr_t;
    typedef std::vector<ptr_t> vector_t;
    static ptr_t Create(const Json& conf) {
        return ptr_t(new Reflect(conf));
    }
    virtual ~Reflect() {
        Destroy();
    }
    virtual bool Initialize() {
        ListenOption opt;
        opt["host"] = conf_["host"].to<std::string>();
        opt["port"] = conf_["port"].to<std::string>();
        opt["backlog"] = conf_["backlog"].to<std::string>("10");
        opt["epolltimeo"] = conf_["epolltimeo"].to<std::string>("100");
        opt.SetSockOpts(conf_["option"], ListenOption::s_sockopts_pre_bind); // "pre-bind" options
        Listener::ptr_t listener(Listener::Create(opt));
        if (!listener->Initialize()) {
            std::cout << prefix(app()) << "Listener::Initialize error: " << listener->GetErrMsg() << std::endl;
            return false;
        }
        listener->AddEvent(shared_from_this(), 0, false);
        listener_.swap(listener);
        std::cout << prefix(app()) << "listen " << opt["host"] << ":" << opt["port"] << std::endl;
        stats_ = conf_["publish"]["stats"].to<int32_t>(0);
        stats_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(stats_);
        return true;
    }
    virtual void Destroy() {
        receivers_.clear();
        listener_.reset();
    }
    virtual std::string app() const {
        return conf_["app"].to<std::string>("live");
    }
protected:
    typedef std::pair<bool, Json> res_t;
    virtual bool PatternMatch(std::string pattern, const std::string& str) const {
        try {
            static const std::string escape = "\\+.?^$-|/{}()[]";
            for (size_t i = 0, c = escape.length(); i < c; ++i) {
                std::string esc = escape.substr(i, 1);
                boost::algorithm::replace_all(pattern, esc, "\\" + esc);
            }
            boost::algorithm::replace_all(pattern, "*", ".*");
            boost::algorithm::replace_all(pattern, "%", ".");
            boost::xpressive::sregex regex = boost::xpressive::sregex::compile("^" + pattern + "$");
            return boost::xpressive::regex_match(str, regex);
        } catch (boost::xpressive::regex_error&) {
            return false;
        }
    }
    virtual bool AccessCheck(const Json::Node& access, const SockAddr& addr, const StreamOption& streamOption) const {
        for (size_t i = 0, c = access.size(); i < c; ++i) {
            std::string name = access[i]["name"].to<std::string>("*");
            if (!name.empty() && name != "*" && !PatternMatch(name, streamOption.ResourceName())) {
                continue;
            }
            std::string deny = access[i]["deny"].to<std::string>();
            if (!deny.empty()) {
                if (deny == "*" || boost::iequals(deny, "all")) return false;
                if (addr.Match(deny)) return false;
            }
            std::string allow = access[i]["allow"].to<std::string>();
            if (!allow.empty()) {
                if (allow == "*" || boost::iequals(allow, "all")) return true;
                if (addr.Match(allow)) return true;
            }
        }
        return true;
    }
    virtual CURLcode Call(const std::string& uri, Json& body) const {
        CurlJsonIO io(curl_);
        io.Reset(5, body);
        curl_easy_setopt(io, CURLOPT_URL, uri.c_str());
        CURLcode res = curl_easy_perform(io);
        body = (res == CURLE_OK) ? io.Json() : Json();
        return res;
    }
    virtual res_t Authorize(const std::string& on, const std::string& call, const SockAddr& addr, const StreamOption& streamOption) const {
        if (on == "on_pre_accept" && !AccessCheck(conf_[call]["access"], addr, streamOption)) return std::make_pair(false, Json());
        std::string uri = conf_[call][on].to<std::string>();
        if (uri.empty()) return std::make_pair(true, Json());
        Json body;
        body["app"] = app().c_str();
        body["on"] = on.c_str();
        body["call"] = call.c_str();
        body["addr"] = addr.ToString().c_str();
        const URIOption::map_t map = streamOption.GetMap();
        for (URIOption::map_t::const_iterator it = map.begin(); it != map.end(); ++it) {
            body["streamid"][it->first] = it->second.c_str();
        }
        CURLcode res = Call(uri, body);
        return std::make_pair(res == CURLE_OK, body);
    }
    virtual bool OnPreAccept(ListenOption& option, int sfd, const SockAddr& peer, const StreamOption& streamOption) override {
        std::string name = streamOption.ResourceName();
        if (name.empty()) {
            return false;
        } else if (streamOption.Mode() == "bidirectional") {
            return false;
        } else if (streamOption.Mode() == "publish") {
            Receiver::ptr_t receiver = FindReceiver(name);
            if (receiver) return false; // already exists
            res_t res = Authorize("on_pre_accept", "publish", peer, streamOption);
            if (!res.first) return false;
            option.SetSockOpts(conf_["option"], ListenOption::s_sockopts_pre); // "pre" options
            option.SetSockOpts(conf_["publish"]["option"], ListenOption::s_sockopts_pre);
            option.SetSockOpts(res.second, ListenOption::s_sockopts_pre);
            return true;
        } else { // "request"
            Receiver::ptr_t receiver = FindReceiver(name);
            if (!receiver) return false; // not exists
            res_t res = Authorize("on_pre_accept", "play", peer, streamOption);
            if (!res.first) return false;
            option.SetSockOpts(conf_["option"], ListenOption::s_sockopts_pre); // "pre" options
            option.SetSockOpts(conf_["play"]["option"], ListenOption::s_sockopts_pre);
            option.SetSockOpts(res.second, ListenOption::s_sockopts_pre);
            return true;
        }
    }
    virtual bool OnAccept(const ListenOption& option, int sfd, const SockAddr& peer, const StreamOption& streamOption) override {
        std::string name = streamOption.ResourceName();
        if (name.empty()) {
            return false;
        } else if (streamOption.Mode() == "bidirectional") {
            return false;
        } else if (streamOption.Mode() == "publish") {
            Receiver::ptr_t receiver = FindReceiver(name);
            if (receiver) return false; // already exists
            ReceiveOption opt;
            opt["name"] = name;
            opt["peer"] = peer.ToString();
            res_t res = Authorize("on_accept", "publish", peer, streamOption);
            if (!res.first) return false;
            opt.SetSockOpts(conf_["option"], ReceiveOption::s_sockopts); // "post" options
            opt.SetSockOpts(conf_["publish"]["option"], ReceiveOption::s_sockopts);
            opt.SetSockOpts(res.second, ReceiveOption::s_sockopts);
            receiver = Receiver::Create(sfd, opt);
            if (!receiver->Initialize()) {
                std::cout << prefix(app()) << "Receiver::Initialize error: " << receiver->GetErrMsg() << std::endl;
                return false;
            }
            receiver->AddEvent(shared_from_this(), 0);
            receivers_[name] = receiver;
            std::cout << prefix(app()) << "accept publish [ " << name << " ] from " << opt["peer"] << std::endl;
            return true;
        } else { // "request"
            Receiver::ptr_t receiver = FindReceiver(name);
            if (!receiver) return false; // not exists
            SendOption opt;
            opt["app"] = app();
            opt["name"] = name;
            opt["peer"] = peer.ToString();
            res_t res = Authorize("on_accept", "play", peer, streamOption);
            if (!res.first) return false;
            opt.SetSockOpts(conf_["option"], SendOption::s_sockopts); // "post" options
            opt.SetSockOpts(conf_["play"]["option"], SendOption::s_sockopts);
            opt.SetSockOpts(res.second, SendOption::s_sockopts);
            Event::ptr_t sender(ReflectSender::Create(sfd, opt));
            receiver->AddEvent(sender, 0, true);
            std::cout << prefix(app()) << "accept request [ " << name << " ] from " << opt["peer"] << std::endl;
            return true;
        }
    }
    virtual bool OnListenerFlag(const ListenOption& option) override {
        for (Receiver::map_t::const_iterator it = receivers_.begin(); it != receivers_.end(); ++it) {
            Receiver::ptr_t receiver = it->second;
            std::string name = receiver->GetOption().Get<std::string>("name");
            std::string stats = receiver->GetStatistics(1, ", ");
            std::cout << prefix(app()) << "stats receive [ " << name << " ] : " << stats << std::endl;
        }
        stats_time_ += std::chrono::seconds(stats_);
        return false;
    }
    virtual bool GetListenerFlag() const override {
        return stats_ > 0 && std::chrono::steady_clock::now() > stats_time_;
    }
    virtual bool OnDisconnected(const ReceiveOption& option) override {
        std::string name = option.Get<std::string>("name");
        std::string peer = option.Get<std::string>("peer");
        std::cout << prefix(app()) << "disconnected [" << name << "] from " << peer << std::endl;
        for (Receiver::map_t::iterator it = receivers_.find(name); it != receivers_.end(); ++it) {
            if (it->second) {
                boost::thread([](Receiver::ptr_t receiver) { receiver.reset(); }, it->second);
                it->second.reset();
            }
        }
        return true;
    }
};

//----------------------------------------------------------------------------
/// @class App
//----------------------------------------------------------------------------
class App
{
    static boost::mutex mutex_;
    static boost::condition_variable cond_;
    Reflect::vector_t reflects_;
public:
    App() : reflects_() {
    }
    virtual ~App() {
        Destroy();
    }
    virtual bool Initialize() {
        if (srt_startup() == SRT_ERROR) {
            return false;
        }
        srt_setloglevel(srt_logging::LogLevel::error);
        srt_setloghandler(this, &App::logHandler);
        return true;
    }
    virtual void Destroy() {
        srt_cleanup();
    }
    virtual int Run(const std::string& conf_file) {
        try {
            Json json = Json::load(conf_file);
            Json::Node reflects = json["reflects"];
            for (size_t i = 0, c = reflects.size(); i < c; ++i) {
                Json conf = reflects[i];
                Json::keys_t keys = conf.to<Json::keys_t>();
                for (Json::keys_t::const_iterator it = keys.begin(); it != keys.end(); ++it) {
                    if (!boost::iequals(*it, "uri")) continue;
                    URI uri = conf[*it].to<std::string>();
                    if (!boost::iequals(uri.scheme, "srt")) continue;
                    URIOption query = uri.query;
                    uri.Decode();
                    conf["host"] = uri.host.c_str();
                    conf["port"] = uri.port.c_str();
                    URIOption::map_t map = query.GetMap();
                    for (URIOption::map_t::const_iterator it = map.begin(); it != map.end(); ++it) {
                        conf["option"][it->first] = it->second.c_str();
                    }
                }
                Reflect::ptr_t reflect = Reflect::Create(conf);
                if (reflect->Initialize()) reflects_.push_back(reflect);
            }
        } catch (std::exception& ex) {
            std::cout << prefix() << "ERROR: " << "failed to read conf [ " << conf_file << "] : " << ex.what() << std::endl;
            return -2;
        }
        if (reflects_.empty()) {
            std::cout << prefix() << "ERROR: " << "there are no reflection entry" << std::endl;
            return -1;
        }
#ifdef SIGBREAK
        std::signal(SIGBREAK, &App::signalHandler);
#endif
        std::signal(SIGINT, &App::signalHandler);
        std::signal(SIGTERM, &App::signalHandler);
        boost::unique_lock<boost::mutex> lk(mutex_);
        cond_.wait(lk);
        return 0;
    }
protected:
    static void signalHandler(int signum) {
        std::cout << prefix() << "signal : " << signum << std::endl;
        boost::unique_lock<boost::mutex> lk(mutex_);
        cond_.notify_all();
    }
    static void logHandler(void* opaque, int level, const char* file, int line, const char* area, const char* message) {
        switch (level) {
            case srt_logging::LogLevel::fatal: {
                std::cout << prefix() << (boost::format("%s(%d): %s : %s : %s\n") % file % line % area % "fatal" % message).str();
                break;
            }
            case srt_logging::LogLevel::error: {
                std::cout << prefix() << (boost::format("%s(%d): %s : %s : %s\n") % file % line % area % "error" % message).str();
                break;
            }
            case srt_logging::LogLevel::warning: {
                std::cout << prefix() << (boost::format("%s(%d): %s : %s : %s\n") % file % line % area % " warn" % message).str();
                break;
            }
            case srt_logging::LogLevel::note: {
                std::cout << prefix() << (boost::format("%s(%d): %s : %s : %s\n") % file % line % area % " note" % message).str();
                break;
            }
            case srt_logging::LogLevel::debug: {
                std::cout << prefix() << (boost::format("%s(%d): %s : %s : %s\n") % file % line % area % "debug" % message).str();
                break;
            }
        }
    }
};

boost::mutex App::mutex_;
boost::condition_variable App::cond_;

int main(int argc, char* argv[]) {
    try {
        std::string conf_file;
        for (int i = 1; i < argc; ++i) {
            if (boost::istarts_with(argv[i], "conf=")) {
                conf_file = std::string(argv[i]).substr(5);
            }
        }
        if (conf_file.empty()) {
            static const char sep[4] = { '\\', '/', '.', 0 };
            conf_file = argv[0];
            size_t pos = conf_file.find_last_of(sep);
            if (pos != std::string::npos && boost::iequals(conf_file.substr(pos), ".exe")) {
                conf_file = conf_file.substr(0, pos);
            }
            conf_file += ".conf";
        }
        App app;
        if (!app.Initialize()) {
            return 1;
        }
        return app.Run(conf_file);
    } catch (std::exception& x) {
        std::cout << prefix() << "exception : " << x.what() << std::endl;
        return -3;
    }
}
