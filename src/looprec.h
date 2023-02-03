#pragma once
#include "event.h"

//----------------------------------------------------------------------------
/// @class LoopRec
//----------------------------------------------------------------------------
class LoopRec : public Event, public boost::enable_shared_from_this<LoopRec>, private boost::noncopyable {
    class Impl;
    boost::scoped_ptr<Impl> pimpl_;
    LoopRec(const Json& conf, const std::string& app, const std::string& name);
public:
    typedef boost::shared_ptr<LoopRec> ptr_t;
    typedef std::map<std::string, ptr_t> map_t;
    static map_t Create(const Json::Node& loopRecs, const std::string& app);
    virtual ~LoopRec();
    virtual bool Initialize();
    virtual void Destroy();
protected:
    virtual bool OnReceive(const ReceiveOption& option, const Event::buf_t& buf, bool discrete) override;
    virtual bool OnDisconnected(const ReceiveOption& option) override;
};
