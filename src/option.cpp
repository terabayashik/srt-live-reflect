#include "stdafx.h"
#include "option.h"

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
bool ListenOption::operator==(const ListenOption& rhs) const {
    static const char* keys[] = {
        "host",
        "port",
        nullptr
    };
    for (int i = 0; keys[i]; ++i) {
        if (!boost::iequals(Get<std::string>(keys[i], ""), rhs.Get<std::string>(keys[i], ""))) {
            return false;
        }
    }
    return true;
}

//----------------------------------------------------------------------------
// Restrict "pre-bind" options
// these options should be set before the srt_bind
//----------------------------------------------------------------------------
const char* ListenOption::s_sockopts_pre_bind[] = {
    "udpsndbuf", "udprcvbuf",
    /*"transtype",*/ /*"pbkeylen",*/ /*"passphrase",*/ "mss", /*"fc",*/ "sndbuf", "rcvbuf", /*"rcvsyn",*/ /*"linger",*/ "ipttl", "iptos",
    /*"latency",*/ /*"tsbpdmode",*/ /*"tlpktdrop",*/ /*"snddropdelay",*/ /*"nakreport",*/ /*"conntimeo",*/ /*"lossmaxttl",*/ /*"rcvlatency",*/ /*"peerlatency",*/ /*"minversion",*/
                                                                                                                                                                  /*"streamid",*/ /*"congestion",*/ /*"messageapi",*/ /*"payloadsize",*/ /*"kmrefreshrate",*/ /*"kmpreannounce",*/ /*"enforcedencryption",*/ /*"peeridletimeo",*/ /*"packetfilter",*/
                                                                                                                                                                  NULL
};
//----------------------------------------------------------------------------
// Restrict "pre" or "post" options
// these options could be set during srt_listen_callback
//----------------------------------------------------------------------------
const char* ListenOption::s_sockopts_pre[] = {
    /*"udpsndbuf",*/ /*"udprcvbuf",*/
    "transtype", "pbkeylen", "passphrase", /*"mss",*/ "fc", /*"sndbuf",*/ /*"rcvbuf",*/ "rcvsyn", "linger", /*"ipttl",*/ /*"iptos",*/
    "latency", "tsbpdmode", "tlpktdrop", "snddropdelay", "nakreport", "conntimeo", "lossmaxttl", "rcvlatency", "peerlatency", "minversion",
    "streamid", "congestion", "messageapi", "payloadsize", "kmrefreshrate", "kmpreannounce", "enforcedencryption", "peeridletimeo", "packetfilter",
#if SRT_VERSION_VALUE >= SRT_MAKE_VERSION_VALUE(1,4,2)
    "retransmitalgo",
#endif
    NULL
};
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
void ListenOption::SetSockOpts(const URIOption& option, const char* sockopts[]) {
    for (int i = 0; sockopts[i]; ++i) {
        if (!option.Has(sockopts[i])) continue;
        operator[](sockopts[i]) = option.Get<std::string>(sockopts[i]);
    }
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
void ListenOption::SetSockOpts(const Json& option, const char* sockopts[]) {
    for (int i = 0; sockopts[i]; ++i) {
        std::string value = option[sockopts[i]].to<std::string>();
        if (!value.empty()) operator[](sockopts[i]) = value;
    }
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
const char* CallOption::s_sockopts[] = {
    "udpsndbuf", "udprcvbuf",
    "transtype", "pbkeylen", "passphrase", "mss", "fc", "sndbuf", "rcvbuf", "rcvsyn", "linger", "ipttl", "iptos",
    "latency", "tsbpdmode", "tlpktdrop", "snddropdelay", "nakreport", "conntimeo", "lossmaxttl", "rcvlatency", "peerlatency", "minversion",
    "streamid", "congestion", "messageapi", "payloadsize", "kmrefreshrate", "kmpreannounce", "enforcedencryption", "peeridletimeo", "packetfilter",
    NULL
};
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
void CallOption::SetSockOpts(const URIOption& option, const char* sockopts[]) {
    for (int i = 0; sockopts[i]; ++i) {
        if (!option.Has(sockopts[i])) continue;
        operator[](sockopts[i]) = option.Get<std::string>(sockopts[i]);
    }
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
void CallOption::SetSockOpts(const Json& option, const char* sockopts[]) {
    for (int i = 0; sockopts[i]; ++i) {
        std::string value = option[sockopts[i]].to<std::string>();
        if (!value.empty()) operator[](sockopts[i]) = value;
    }
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
bool ReceiveOption::operator==(const ReceiveOption& rhs) const {
    static const char* keys[] = {
        "streamname",
        nullptr
    };
    for (int i = 0; keys[i]; ++i) {
        if (!boost::iequals(Get<std::string>(keys[i], ""), rhs.Get<std::string>(keys[i], ""))) {
            return false;
        }
    }
    return true;
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
const char* ReceiveOption::s_sockopts[] = {
    "maxbw", "inputbw", "oheadbw", "rcvtimeo", "sndtimeo", "sndsyn",
#if SRT_VERSION_VALUE >= SRT_MAKE_VERSION_VALUE(1,4,3)
    "mininputbw",
#endif
    NULL
};
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
void ReceiveOption::SetSockOpts(const URIOption& option, const char* sockopts[]) {
    for (int i = 0; sockopts[i]; ++i) {
        if (!option.Has(sockopts[i])) continue;
        operator[](sockopts[i]) = option.Get<std::string>(sockopts[i]);
    }
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
void ReceiveOption::SetSockOpts(const Json& option, const char* sockopts[]) {
    for (int i = 0; sockopts[i]; ++i) {
        std::string value = option[sockopts[i]].to<std::string>();
        if (!value.empty()) operator[](sockopts[i]) = value;
    }
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
const char* SendOption::s_sockopts[] = {
    "maxbw", "inputbw", "oheadbw", "rcvtimeo", "sndtimeo", "sndsyn",
#if SRT_VERSION_VALUE >= SRT_MAKE_VERSION_VALUE(1,4,3)
    "mininputbw",
#endif
    NULL
};
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
void SendOption::SetSockOpts(const URIOption& option, const char* sockopts[]) {
    for (int i = 0; sockopts[i]; ++i) {
        if (!option.Has(sockopts[i])) continue;
        operator[](sockopts[i]) = option.Get<std::string>(sockopts[i]);
    }
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
void SendOption::SetSockOpts(const Json& option, const char* sockopts[]) {
    for (int i = 0; sockopts[i]; ++i) {
        std::string value = option[sockopts[i]].to<std::string>();
        if (!value.empty()) operator[](sockopts[i]) = value;
    }
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
StreamOption& StreamOption::ParseStreamId(const char* streamid) {
    if (streamid && streamid[0]) {
        if (boost::algorithm::starts_with(streamid, "#!::")) {
            // "#!::r=resourcename,u=username,m=request..."
            this->Parse(streamid + strlen("#!::"), ",", "=");
        } else if (boost::algorithm::starts_with(streamid, "%23!::")) {
            // "%23!::r=resourcename,u=username,m=request..." (lack of pct-decode)
            this->Parse(streamid + strlen("%23!::"), ",", "=");
        } else {
            // "resourcename;u=username,m=request..."
            const char* pos = strchr(streamid, ';');
            this->Parse(pos ? pos + 1 : "", ",", "=");
            (*this)["r"] = DecodeURI(streamid, pos ? (pos - streamid) : strlen(streamid));
        }
    }
    return *this;
}
