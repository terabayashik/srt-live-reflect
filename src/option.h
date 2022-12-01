#pragma once

#include "URI.h"
#include "json.h"

//----------------------------------------------------------------------------
/// @class ListenOption
/// options for CListener
//----------------------------------------------------------------------------
class ListenOption : public URIOption
{
public:
    static const char* s_sockopts_pre_bind[]; // options which should be set before srt_bind
    static const char* s_sockopts_pre[]; // options which could be set during srt_listen_callback
    ListenOption() : URIOption() { operator[]("rcvsyn") = "0"; } // NON_BLOCKING
    virtual bool operator==(const ListenOption& rhs) const; // "host", "port"
    virtual void SetSockOpts(const URIOption& option, const char* sockopts[]);
    virtual void SetSockOpts(const Json& option, const char* sockopts[]);
};
//----------------------------------------------------------------------------
/// @class CallOption
/// options for CCaller
//----------------------------------------------------------------------------
class CallOption : public URIOption
{
public:
    static const char* s_sockopts[];
    CallOption() : URIOption() {}
    virtual void SetSockOpts(const URIOption& option, const char* sockopts[]);
    virtual void SetSockOpts(const Json& option, const char* sockopts[]);
};
//----------------------------------------------------------------------------
/// @class ReceiveOption
/// options for CReceiver
//----------------------------------------------------------------------------
class ReceiveOption : public URIOption
{
public:
    static const char* s_sockopts[];
    ReceiveOption() : URIOption() { operator[]("rcvsyn") = "0"; } // NON_BLOCKING
    virtual bool operator==(const ReceiveOption& rhs) const; // compare for "streamname"
    virtual void SetSockOpts(const URIOption& option, const char* sockopts[]);
    virtual void SetSockOpts(const Json& option, const char* sockopts[]);
};
//----------------------------------------------------------------------------
/// @class SendOption
/// options for CSender
//----------------------------------------------------------------------------
class SendOption : public URIOption
{
public:
    static const char* s_sockopts[];
    SendOption() : URIOption() { operator[]("sndsyn") = "0"; } // NON_BLOCKING
    virtual void SetSockOpts(const URIOption& option, const char* sockopts[]);
    virtual void SetSockOpts(const Json& option, const char* sockopts[]);
};
//----------------------------------------------------------------------------
/// @class StreamOption
/// SRTO_STREAMID
/// key1=val1,key2=val2... instead of key1=val1&key2=val2...
/// refs) https://github.com/Haivision/srt/blob/master/docs/AccessControl.md#standard-keys
//----------------------------------------------------------------------------
class StreamOption : public URIOption
{
public:
    StreamOption(const char* streamid = nullptr) : URIOption() { ParseStreamId(streamid); }
    virtual StreamOption& ParseStreamId(const char* streamid);
    virtual std::string ResourceName() const { return Get<std::string>("r", "", ""); }
    virtual std::string UserName() const { return Get<std::string>("u", "", ""); }
    virtual std::string HostName() const { return Get<std::string>("h", "", ""); }
    virtual std::string SessionID() const { return Get<std::string>("s", "", ""); }
    virtual std::string Type() const { return Get<std::string>("t", "", ""); }
    virtual std::string Mode() const { return Get<std::string>("m", "", ""); }
};
