#include "stdafx.h"
#include "sockaddr.h"

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
SockAddr::SockAddr() {
    memset(static_cast<sockaddr_storage*>(this), 0, sizeof(sockaddr_storage));
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
SockAddr::SockAddr(const SockAddr& rhs) {
    memcpy(static_cast<sockaddr_storage*>(this), static_cast<const sockaddr_storage*>(&rhs), sizeof(sockaddr_storage));
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
SockAddr::SockAddr(const sockaddr_storage& rhs) {
    memcpy(static_cast<sockaddr_storage*>(this), &rhs, sizeof(sockaddr_storage));
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
SockAddr::SockAddr(const char* host, const char* port, int skip) {
    memset(static_cast<sockaddr_storage*>(this), 0, sizeof(sockaddr_storage));
    addrinfo* res0 = nullptr;
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, port, &hints, &res0)) return;
    for (addrinfo* res = res0; res; res = res->ai_next) {
        if (skip--) continue;
        memcpy(static_cast<sockaddr_storage*>(this), res->ai_addr, std::min<int>(res->ai_addrlen, sizeof(sockaddr_storage)));
        break;
    }
    freeaddrinfo(res0);
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
SockAddr::SockAddr(const sockaddr* sa, int len) {
    memset(static_cast<sockaddr_storage*>(this), 0, sizeof(sockaddr_storage));
    if (len < 0) {
        switch (sa->sa_family) {
            case AF_INET: len = sizeof(sockaddr_in); break;
            case AF_INET6: len = sizeof(sockaddr_in6); break;
            default: return;
        }
    }
    memcpy(static_cast<sockaddr_storage*>(this), sa, std::min<int>(len, sizeof(sockaddr_storage)));
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
SockAddr& SockAddr::operator=(const SockAddr& rhs) {
    if (static_cast<sockaddr_storage*>(this) == static_cast<const sockaddr_storage*>(&rhs)) return *this;
    memcpy(static_cast<sockaddr_storage*>(this), static_cast<const sockaddr_storage*>(&rhs), sizeof(sockaddr_storage));
    return *this;
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
SockAddr& SockAddr::operator=(const sockaddr_storage& rhs) {
    if (static_cast<sockaddr_storage*>(this) == &rhs) return *this;
    memcpy(static_cast<sockaddr_storage*>(this), &rhs, sizeof(sockaddr_storage));
    return *this;
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
sockaddr_storage* SockAddr::operator&() {
    return static_cast<sockaddr_storage*>(this);
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
const sockaddr_storage* SockAddr::operator&() const {
    return static_cast<const sockaddr_storage*>(this);
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
std::string SockAddr::ToString() const {
    char hname[NI_MAXHOST] = { '\0' };
    char sname[NI_MAXSERV] = { '\0' };
    int flags = NI_NUMERICHOST | NI_NUMERICSERV;
    const sockaddr* p = reinterpret_cast<const sockaddr*>(static_cast<const sockaddr_storage*>(this));
    if (getnameinfo(p, sizeof(sockaddr_storage), hname, sizeof(hname), sname, sizeof(sname), flags)) {
        return "";
    } else {
        std::stringstream ss;
        if (ss_family == AF_INET6) {
            ss << "[" << hname << "]:" << sname;
        } else {
            ss << hname << ":" << sname;
        }
        return ss.str();
    }
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
std::string SockAddr::GetAddress() const {
    char hname[NI_MAXHOST] = { '\0' };
    int flags = NI_NUMERICHOST;
    const sockaddr* p = reinterpret_cast<const sockaddr*>(static_cast<const sockaddr_storage*>(this));
    if (getnameinfo(p, sizeof(sockaddr_storage), hname, sizeof(hname), nullptr, 0, flags)) {
        return "";
    } else {
        std::stringstream ss;
        ss << hname;
        return ss.str();
    }
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int SockAddr::GetPort() const {
    char sname[NI_MAXSERV] = { '\0' };
    int flags = NI_NUMERICSERV;
    const sockaddr* p = reinterpret_cast<const sockaddr*>(static_cast<const sockaddr_storage*>(this));
    if (getnameinfo(p, sizeof(sockaddr_storage), nullptr, 0, sname, sizeof(sname), flags)) {
        return -1;
    } else {
        return atoi(sname);
    }
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
bool SockAddr::GetSockName(int sfd) {
    sockaddr* p = reinterpret_cast<sockaddr*>(static_cast<sockaddr_storage*>(this));
    int len = sizeof(sockaddr_storage);
    return srt_getsockname(static_cast<SRTSOCKET>(sfd), p, &len) != SRT_ERROR;
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
bool SockAddr::GetPeerName(int sfd) {
    sockaddr* p = reinterpret_cast<sockaddr*>(static_cast<sockaddr_storage*>(this));
    int len = sizeof(sockaddr_storage);
    return srt_getsockname(static_cast<SRTSOCKET>(sfd), p, &len) != SRT_ERROR;
}
//----------------------------------------------------------------------------
//  convert IPv4-mapped IPv6 address to IPv4 address
//----------------------------------------------------------------------------
bool SockAddr::ConvertV4MappedV6ToV4() {
    if (!IsV4MappedV6()) return false;
    sockaddr_in sin = { 0 };
    sin.sin_family = AF_INET;
    sin.sin_port = reinterpret_cast<const sockaddr_in6*>(this)->sin6_port;
    sin.sin_addr = *reinterpret_cast<const in_addr*>(reinterpret_cast<const sockaddr_in6*>(this)->sin6_addr.s6_addr + 12);
    memset(static_cast<sockaddr_storage*>(this), 0, sizeof(sockaddr_storage));
    memcpy(static_cast<sockaddr_storage*>(this), &sin, sizeof(sockaddr_in));
    return true;
}
//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
bool SockAddr::Match(const std::string& condition) const {
    const size_t pos = condition.find_first_of('/');
    SockAddr addr(condition.substr(0, pos).c_str(), nullptr);
    if (addr.ss_family != ss_family) return false;
    int32_t len = -1;
    if (pos != std::string::npos) {
        try {
            len = boost::lexical_cast<int32_t>(condition.substr(pos + 1));
        } catch (boost::bad_lexical_cast&) {
            return false;
        }
    }
    if (ss_family == AF_INET) {
        if (len < 0) len = 32;
        if (len == 0) return true;
        uint32_t mask = (len < 32) ? (0xffffffffu << (32 - len)) : 0xffffffffu;
        return (htonl(reinterpret_cast<const sockaddr_in*>(this)->sin_addr.s_addr) & mask)
            == (htonl(reinterpret_cast<const sockaddr_in*>(&addr)->sin_addr.s_addr) & mask);
    } else if (ss_family == AF_INET6) {
        if (len < 0) len = 128;
        const uint8_t* octet1 = reinterpret_cast<const sockaddr_in6*>(this)->sin6_addr.s6_addr;
        const uint8_t* octet2 = reinterpret_cast<const sockaddr_in6*>(&addr)->sin6_addr.s6_addr;
        for (int32_t c = 16; c && len > 0; --c, ++octet1, ++octet2, len -= 8) {
            uint8_t mask = (len < 8) ? (0xffu << (8 - len)) : 0xffu;
            if ((*octet1 & mask) != (*octet2 & mask)) return false;
        }
        return true;
    } else {
        return false;
    }
}
