#pragma once

#include "option.h"
#include "event.h"

//----------------------------------------------------------------------------
/// @class Listener
//----------------------------------------------------------------------------
class Listener : public boost::enable_shared_from_this<Listener>, private boost::noncopyable
{
    class Impl;
    boost::scoped_ptr<Impl> pimpl_;
    Listener(const ListenOption& listenOption);
public:
    typedef boost::shared_ptr<Listener> ptr_t;
    static ptr_t Create(const ListenOption& listenOption);
    virtual ~Listener(void);
    virtual bool Initialize();
    virtual void Destroy();
    virtual void AddEvent(Event::wptr_t ev, int priority, bool own = false);
    virtual const ListenOption& GetOption() const;
    virtual std::string GetErrMsg(const std::string& sep = "\n") const;
};
