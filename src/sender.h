#pragma once

#include "option.h"
#include "event.h"

//----------------------------------------------------------------------------
/// @class Sender
//----------------------------------------------------------------------------
class Sender : public boost::enable_shared_from_this<Sender>, private boost::noncopyable
{
    class Impl;
    boost::scoped_ptr<Impl> pimpl_;
    Sender(int sfd, const SendOption& option);
public:
    typedef boost::shared_ptr<Sender> ptr_t;
    static ptr_t Create(int sfd, const SendOption& option);
    virtual ~Sender();
    virtual bool Initialize();
    virtual void Destroy();
    virtual bool Send(const Event::buf_t& buf);
    virtual bool Send(const char* buf, size_t len);
    virtual const SendOption& GetOption() const;
    virtual std::string GetErrMsg(const std::string& sep = "\n") const;
    virtual std::string GetStatistics(int level = 1, const std::string& sep = "\n") const;
};
