#pragma once

#include "option.h"
#include "event.h"

//----------------------------------------------------------------------------
/// @class CReceiver
//----------------------------------------------------------------------------
class Receiver : public boost::enable_shared_from_this<Receiver>, private boost::noncopyable
{
    class Impl;
    boost::scoped_ptr<Impl> pimpl_;
    Receiver(int sfd, const ReceiveOption& receiveOption);
public:
    typedef boost::shared_ptr<Receiver> ptr_t;
    typedef std::map<std::string, ptr_t> map_t;
    static ptr_t Create(int sfd, const ReceiveOption& receiveOption);
    virtual ~Receiver();
    virtual bool Initialize();
    virtual void Destroy();
    virtual void AddEvent(Event::wptr_t ev, int priority, bool own = false);
    virtual const ReceiveOption& GetOption() const;
    virtual std::string GetErrMsg(const std::string& sep = "\n") const;
    virtual std::string GetStatistics(int level = 1, const std::string& sep = "\n") const;
};
