#pragma once

//----------------------------------------------------------------------------
/// @class SockAddr
//----------------------------------------------------------------------------
class SockAddr : public sockaddr_storage
{
public:
    SockAddr();
    explicit SockAddr(const SockAddr& rhs);
    explicit SockAddr(const sockaddr_storage& rhs);
    explicit SockAddr(const char* host, const char* port, int skip = 0);
    explicit SockAddr(const sockaddr* sa, size_t len = -1);
    SockAddr& operator=(const SockAddr& rhs);
    SockAddr& operator=(const sockaddr_storage& rhs);
    sockaddr_storage* operator&();
    const sockaddr_storage* operator&() const;
    std::string ToString() const;
    std::string GetAddress() const;
    int GetPort() const;
    bool GetSockName(int sfd);
    bool GetPeerName(int sfd);
    bool IsV4() const { return this->ss_family == AF_INET; }
    bool IsV6() const { return this->ss_family == AF_INET6; }
    bool IsV4MappedV6() const { return this->ss_family == AF_INET6 && IN6_IS_ADDR_V4MAPPED(&reinterpret_cast<const sockaddr_in6*>(this)->sin6_addr); }
    bool ConvertV4MappedV6ToV4();
    bool Match(const std::string& condition) const;
};
