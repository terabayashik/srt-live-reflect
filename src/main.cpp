#include "stdafx.h"
#include "listener.h"
#include "receiver.h"
#include "sender.h"

//----------------------------------------------------------------------------
/// @class ReflectSender
//----------------------------------------------------------------------------
class ReflectSender : public Event {
    Sender::ptr_t sender_;
    std::string name_;
    std::string peer_;
protected:
    ReflectSender(int sfd, const SendOption& option) : Event(), sender_(Sender::Create(sfd, option)) {
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
    bool OnReceive(const ReceiveOption& option, const Event::buf_t& buf, bool discrete) {
        if (!sender_) return false;
        if (sender_->Send(buf)) return true;
        std::string err = sender_->GetErrMsg();
        sender_.reset();
        std::cout << "send failed [" << name_ << "] for " << peer_ << " : " << err << std::endl;
        return false;
    }
};

//----------------------------------------------------------------------------
/// @class Reflect
//----------------------------------------------------------------------------
class Reflect : public Event, public boost::enable_shared_from_this<Reflect>, private boost::noncopyable
{
    URIOption option_;
    Listener::ptr_t listener_;
    Receiver::map_t receivers_;
protected:
    Reflect(URIOption option) : Event(), option_(option), listener_(), receivers_() {
    }
    Receiver::ptr_t FindReceiver(const std::string& name) const {
        const Receiver::map_t::const_iterator it = receivers_.find(name);
        return it == receivers_.end() ? Receiver::ptr_t() : it->second;
    }
public:
    typedef boost::shared_ptr<Reflect> ptr_t;
    typedef std::vector<ptr_t> vector_t;
    static ptr_t Create(const URIOption& option) {
        return ptr_t(new Reflect(option));
    }
    virtual ~Reflect() {
        Destroy();
    }
    virtual bool Initialize() {
        ListenOption opt;
        opt["host"] = option_.Get<std::string>("host");
        opt["port"] = option_.Get<std::string>("port");
        opt["backlog"] = option_.Get<std::string>("backlog", "10");
        opt["epolltimeo"] = option_.Get<std::string>("epolltimeo", "100");
        opt.SetSockOpts(option_, ListenOption::s_sockopts_pre_bind); // "pre-bind" options
        Listener::ptr_t listener(Listener::Create(opt));
        if (!listener->Initialize()) {
            std::cout << "Listener::Initialize error: " << listener->GetErrMsg() << std::endl;
            return false;
        }
        listener->AddEvent(shared_from_this(), 0, false);
        listener_.swap(listener);
        return true;
    }
    virtual void Destroy() {
        receivers_.clear();
        listener_.reset();
    }
    virtual bool OnPreAccept(ListenOption& option, int sfd, const SockAddr& peer, const StreamOption& streamOption) {
        std::string name = streamOption.ResourceName();
        if (name.empty()) {
            return false;
        } else if (streamOption.Mode() == "bidirectional") {
            return false;
        } else if (streamOption.Mode() == "publish") {
            Receiver::ptr_t receiver = FindReceiver(name);
            if (receiver) return false; // already exists
            option.SetSockOpts(option_, ListenOption::s_sockopts_pre); // "pre" options
            return true;
        } else { // "request"
            Receiver::ptr_t receiver = FindReceiver(name);
            if (!receiver) return false; // not exists
            option.SetSockOpts(option_, ListenOption::s_sockopts_pre); // "pre" options
            return true;
        }
    }
    virtual bool OnAccept(const ListenOption& option, int sfd, const SockAddr& peer, const StreamOption& streamOption) {
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
            opt.SetSockOpts(option_, ReceiveOption::s_sockopts); // "post" options
            receiver = Receiver::Create(sfd, opt);
            if (!receiver->Initialize()) {
                std::cout << "Receiver::Initialize error: " << receiver->GetErrMsg() << std::endl;
                return false;
            }
            receiver->AddEvent(shared_from_this(), 0);
            receivers_[name] = receiver;
            std::cout << "accept publish [ " << name << " ] from " << opt["peer"] << std::endl;
            return true;
        } else { // "request"
            Receiver::ptr_t receiver = FindReceiver(name);
            if (!receiver) return false; // not exists
            SendOption opt;
            opt["name"] = name;
            opt["peer"] = peer.ToString();
            opt.SetSockOpts(option_, SendOption::s_sockopts); // "post" options
            Event::ptr_t sender(ReflectSender::Create(sfd, opt));
            receiver->AddEvent(sender, 0, true);
            std::cout << "accept request [ " << name << " ] from " << opt["peer"] << std::endl;
            return true;
        }
    }
    virtual bool OnDisconnected(const ReceiveOption& option) {
        std::string name = option.Get<std::string>("name");
        std::string peer = option.Get<std::string>("peer");
        std::cout << "disconnected [" << name << "] from " << peer << std::endl;
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
            boost::property_tree::ptree conf;
            boost::property_tree::read_json(conf_file, conf);
            boost::property_tree::ptree it = conf.get_child("reflects");
            for (boost::property_tree::ptree::const_iterator it2 = it.begin(); it2 != it.end(); ++it2) {
                boost::optional<std::string> u = it2->second.get_optional<std::string>("URI");
                URIOption option;
                if (u) {
                    URI uri(u.get());
                    std::string q = uri.query;
                    uri.Decode();
                    if (boost::iequals(uri.scheme, "srt")) {
                        option = q;
                        option["host"] = uri.host;
                        option["port"] = uri.port;
                    }
                }
                for (boost::property_tree::ptree::const_iterator it3 = it2->second.begin(); it3 != it2->second.end(); ++it3) {
                    option[it3->first] = it3->second.data();
                }
                Reflect::ptr_t reflect = Reflect::Create(option);
                if (reflect->Initialize()) reflects_.push_back(reflect);
            }
        } catch (std::exception& ex) {
            std::cout << "ERROR: " << "failed to read conf: " << ex.what() << std::endl;
            return -2;
        }
        if (reflects_.empty()) {
            std::cout << "ERROR: " << "there are no reflection entry" << std::endl;
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
        std::cout << "signal : " << signum << std::endl;
        boost::unique_lock<boost::mutex> lk(mutex_);
        cond_.notify_all();
    }
    static void logHandler(void* opaque, int level, const char* file, int line, const char* area, const char* message) {
        switch (level) {
            case srt_logging::LogLevel::fatal: {
                std::cout << (boost::format("%s(%d): %s : %s : %s\n") % file % line % area % "fatal" % message).str();
                break;
            }
            case srt_logging::LogLevel::error: {
                std::cout << (boost::format("%s(%d): %s : %s : %s\n") % file % line % area % "error" % message).str();
                break;
            }
            case srt_logging::LogLevel::warning: {
                std::cout << (boost::format("%s(%d): %s : %s : %s\n") % file % line % area % " warn" % message).str();
                break;
            }
            case srt_logging::LogLevel::note: {
                std::cout << (boost::format("%s(%d): %s : %s : %s\n") % file % line % area % " note" % message).str();
                break;
            }
            case srt_logging::LogLevel::debug: {
                std::cout << (boost::format("%s(%d): %s : %s : %s\n") % file % line % area % "debug" % message).str();
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
            conf_file = std::string(argv[0]) + ".conf";
        }
        App app;
        if (!app.Initialize()) {
            return 1;
        }
        return app.Run(conf_file);
    } catch (std::exception& x) {
        std::cout << "exception : " << x.what() << std::endl;
        return -3;
    }
}
