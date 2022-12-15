﻿#include "stdafx.h"
#include "sender.h"
#include "messages.h"

//----------------------------------------------------------------------------
/// @class Sender::Impl
//----------------------------------------------------------------------------
class Sender::Impl
{
    Sender* owner_;
    SRTSOCKET sfd_;
    const SendOption option_;
    SafeMessages errmsgs_;
public:
    Impl(Sender* owner, SRTSOCKET sfd, const SendOption& option)
        : owner_(owner), sfd_(sfd), option_(option), errmsgs_() {
    }
    virtual ~Impl() {
        Destroy();
    }
    virtual bool Initialize() {
        if (sfd_ == SRT_INVALID_SOCK) {
            errmsgs_ << "invalid socket.";
            return false;
        }
        if (option_.Has("maxbw")) {
            int64_t maxbw = option_.Get<int64_t>("maxbw", -1);
            if (srt_setsockflag(sfd_, SRTO_MAXBW, &maxbw, sizeof(maxbw)) == SRT_ERROR) {
                errmsgs_ << boost::format("failed srt_setsockflag(SRTO_MAXBW) [ %lld ]; %s") % maxbw % srt_getlasterror_str();
                return false;
            }
        }
        if (option_.Has("inputbw")) {
            int64_t inputbw = option_.Get<int64_t>("inputbw", 0);
            if (srt_setsockflag(sfd_, SRTO_INPUTBW, &inputbw, sizeof(inputbw)) == SRT_ERROR) {
                errmsgs_ << boost::format("failed srt_setsockflag(SRTO_INPUTBW) [ %lld ]; %s") % inputbw % srt_getlasterror_str();
                return false;
            }
        }
#if SRT_VERSION_VALUE >= SRT_MAKE_VERSION_VALUE(1,4,3)
        if (option_.Has("mininputbw")) {
            int64_t mininputbw = option_.Get<int32_t>("mininputbw", 0);
            if (srt_setsockflag(sfd_, SRTO_MININPUTBW, &mininputbw, sizeof(mininputbw)) == SRT_ERROR) {
                errmsgs_ << boost::format("failed srt_setsockflag(SRTO_MININPUTBW) [ %d ]; %s") % mininputbw % srt_getlasterror_str();
                return false;
            }
        }
#endif
        if (option_.Has("oheadbw")) {
            int oheadbw = option_.Get<int>("oheadbw", 25);
            if (srt_setsockflag(sfd_, SRTO_OHEADBW, &oheadbw, sizeof(oheadbw)) == SRT_ERROR) {
                errmsgs_ << boost::format("failed srt_setsockflag(SRTO_OHEADBW) [ %d ]; %s") % oheadbw % srt_getlasterror_str();
                return false;
            }
        }
        if (option_.Has("rcvtimeo")) {
            int rcvtimeo = option_.Get<int>("rcvtimeo", -1);
            if (srt_setsockflag(sfd_, SRTO_RCVTIMEO, &rcvtimeo, sizeof(rcvtimeo)) == SRT_ERROR) {
                errmsgs_ << boost::format("failed srt_setsockflag(SRTO_RCVTIMEO) [ %d ]; %s") % rcvtimeo % srt_getlasterror_str();
                return false;
            }
        }
        if (option_.Has("sndtimeo")) {
            int sndtimeo = option_.Get<int>("sndtimeo", -1);
            if (srt_setsockflag(sfd_, SRTO_SNDTIMEO, &sndtimeo, sizeof(sndtimeo)) == SRT_ERROR) {
                errmsgs_ << boost::format("failed srt_setsockflag(SRTO_SNDTIMEO) [ %d ]; %s") % sndtimeo % srt_getlasterror_str();
                return false;
            }
        }
        if (option_.Has("sndsyn")) {
            bool sndsyn = option_.Get<int>("sndsyn", 0) > 0 ? true : false;
            if (srt_setsockflag(sfd_, SRTO_SNDSYN, &sndsyn, sizeof(sndsyn)) == SRT_ERROR) {
                errmsgs_ << boost::format("failed srt_setsockflag(SRTO_SNDSYN) [ %s ]; %s") % (sndsyn ? "true" : "false") % srt_getlasterror_str();
                return false;
            }
        }
        return true;
    }
    virtual void Destroy() {
        if (sfd_ != SRT_INVALID_SOCK) {
            srt_close(sfd_);
            sfd_ = SRT_INVALID_SOCK;
        }
    }
    virtual bool Send(const char* buf, size_t len) {
        if (sfd_ == SRT_INVALID_SOCK) {
            return false;
        }
        if (srt_send(sfd_, buf, static_cast<int>(len)) == SRT_ERROR) {
            if (srt_getlasterror(nullptr) != SRT_EASYNCSND) {
                errmsgs_ << boost::format("failed srt_send(): %s") % srt_getlasterror_str();
                return false;
            }
        }
        return true;
    }
    virtual const SendOption& GetOption() const {
        return option_;
    }
    virtual std::string GetErrMsg(const std::string& sep) const {
        return errmsgs_(sep);
    }
    virtual std::string GetStatistics(int level, const std::string& sep = "\n") const {
        if (level <= 0) return "";
        SRT_TRACEBSTATS perf = { 0 };
        if (srt_bstats(sfd_, &perf, 1) == SRT_ERROR) {
            return srt_getlasterror_str();
        }
#define COMMON(l, x)	((l >= (0 + x)))
#define SNDR_O(l, x)	((l >= (0 + x)))
#define RCVR_O(l, x)	((l >= (5 + x)))
        std::stringstream ss;
        // global measurements
        if (COMMON(level, 1)) ss << "msTimeStamp:" << perf.msTimeStamp << sep;							// time since the UDT entity is started, in milliseconds
        if (SNDR_O(level, 1)) ss << "pktSentTotal:" << perf.pktSentTotal << sep;						// total number of sent data packets, including retransmissions
        if (RCVR_O(level, 1)) ss << "pktRecvTotal:" << perf.pktRecvTotal << sep;						// total number of received packets
#if SRT_VERSION_VALUE >= SRT_MAKE_VERSION_VALUE(1,4,2)
        if (SNDR_O(level, 1)) ss << "pktSentUniqueTotal:" << perf.pktSentUniqueTotal << sep;			// total number of data packets sent by the application
        if (RCVR_O(level, 1)) ss << "pktRecvUniqueTotal:" << perf.pktRecvUniqueTotal << sep;			// total number of packets to be received by the application
#endif
        if (SNDR_O(level, 1)) ss << "pktSndLossTotal:" << perf.pktSndLossTotal << sep;					// total number of lost packets (sender side)
        if (RCVR_O(level, 1)) ss << "pktRcvLossTotal:" << perf.pktRcvLossTotal << sep;					// total number of lost packets (receiver side)
        if (SNDR_O(level, 1)) ss << "pktRetransTotal:" << perf.pktRetransTotal << sep;					// total number of retransmitted packets
        if (RCVR_O(level, 1)) ss << "pktSentACKTotal:" << perf.pktSentACKTotal << sep;					// total number of sent ACK packets
        if (SNDR_O(level, 1)) ss << "pktRecvACKTotal:" << perf.pktRecvACKTotal << sep;					// total number of received ACK packets
        if (RCVR_O(level, 1)) ss << "pktSentNAKTotal:" << perf.pktSentNAKTotal << sep;					// total number of sent NAK packets
        if (SNDR_O(level, 1)) ss << "pktRecvNAKTotal:" << perf.pktRecvNAKTotal << sep;					// total number of received NAK packets
        if (SNDR_O(level, 1)) ss << "usSndDurationTotal:" << perf.usSndDurationTotal << sep;			// total time duration when UDT is sending data (idle time exclusive)
        if (SNDR_O(level, 1)) ss << "pktSndDropTotal:" << perf.pktSndDropTotal << sep;					// number of too-late-to-send dropped packets
        if (RCVR_O(level, 1)) ss << "pktRcvDropTotal:" << perf.pktRcvDropTotal << sep;					// number of too-late-to play missing packets
        if (RCVR_O(level, 1)) ss << "pktRcvUndecryptTotal:" << perf.pktRcvUndecryptTotal << sep;		// number of undecrypted packets
        if (SNDR_O(level, 2)) ss << "pktSndFilterExtraTotal:" << perf.pktSndFilterExtraTotal << sep;	// number of control packets supplied by packet filter
        if (RCVR_O(level, 2)) ss << "pktRcvFilterExtraTotal:" << perf.pktRcvFilterExtraTotal << sep;	// number of control packets received and not supplied back
        if (RCVR_O(level, 2)) ss << "pktRcvFilterSupplyTotal:" << perf.pktRcvFilterSupplyTotal << sep;	// number of packets that the filter supplied extra (e.g. FEC rebuilt)
        if (RCVR_O(level, 2)) ss << "pktRcvFilterLossTotal:" << perf.pktRcvFilterLossTotal << sep;		// number of packet loss not coverable by filter
        if (SNDR_O(level, 2)) ss << "byteSentTotal:" << perf.byteSentTotal << sep;						// total number of sent data bytes, including retransmissions
        if (RCVR_O(level, 2)) ss << "byteRecvTotal:" << perf.byteRecvTotal << sep;						// total number of received bytes
#if SRT_VERSION_VALUE >= SRT_MAKE_VERSION_VALUE(1,4,2)
        if (SNDR_O(level, 2)) ss << "byteSentUniqueTotal:" << perf.byteSentUniqueTotal << sep;			// total number of data bytes, sent by the application
        if (RCVR_O(level, 2)) ss << "byteRecvUniqueTotal:" << perf.byteRecvUniqueTotal << sep;			// total number of data bytes to be received by the application
#endif
        if (RCVR_O(level, 2)) ss << "byteRcvLossTotal:" << perf.byteRcvLossTotal << sep;				// total number of lost bytes
        if (SNDR_O(level, 2)) ss << "byteRetransTotal:" << perf.byteRetransTotal << sep;				// total number of retransmitted bytes
        if (SNDR_O(level, 2)) ss << "byteSndDropTotal:" << perf.byteSndDropTotal << sep;				// number of too-late-to-send dropped bytes
        if (RCVR_O(level, 2)) ss << "byteRcvDropTotal:" << perf.byteRcvDropTotal << sep;				// number of too-late-to play missing bytes (estimate based on average packet size)
        if (RCVR_O(level, 2)) ss << "byteRcvUndecryptTotal:" << perf.byteRcvUndecryptTotal << sep;		// number of undecrypted bytes
                                                                                                        // local measurements
        if (SNDR_O(level, 1)) ss << "pktSent:" << perf.pktSent << sep;									// number of sent data packets, including retransmissions
        if (RCVR_O(level, 1)) ss << "pktRecv:" << perf.pktRecv << sep;									// number of received packets
#if SRT_VERSION_VALUE >= SRT_MAKE_VERSION_VALUE(1,4,2)
        if (SNDR_O(level, 1)) ss << "pktSentUnique:" << perf.pktSentUnique << sep;						// number of data packets sent by the application
        if (RCVR_O(level, 1)) ss << "pktRecvUnique:" << perf.pktRecvUnique << sep;						// number of packets to be received by the application
#endif
        if (SNDR_O(level, 1)) ss << "pktSndLoss:" << perf.pktSndLoss << sep;							// number of lost packets (sender side)
        if (RCVR_O(level, 1)) ss << "pktRcvLoss:" << perf.pktRcvLoss << sep;							// number of lost packets (receiver side)
        if (SNDR_O(level, 1)) ss << "pktRetrans:" << perf.pktRetrans << sep;							// number of retransmitted packets
        if (RCVR_O(level, 1)) ss << "pktRcvRetrans:" << perf.pktRcvRetrans << sep;						// number of retransmitted packets received
        if (RCVR_O(level, 1)) ss << "pktSentACK:" << perf.pktSentACK << sep;							// number of sent ACK packets
        if (SNDR_O(level, 1)) ss << "pktRecvACK:" << perf.pktRecvACK << sep;							// number of received ACK packets
        if (RCVR_O(level, 1)) ss << "pktSentNAK:" << perf.pktSentNAK << sep;							// number of sent NAK packets
        if (SNDR_O(level, 1)) ss << "pktRecvNAK:" << perf.pktRecvNAK << sep;							// number of received NAK packets
        if (SNDR_O(level, 2)) ss << "pktSndFilterExtra:" << perf.pktSndFilterExtra << sep;				// number of control packets supplied by packet filter
        if (RCVR_O(level, 2)) ss << "pktRcvFilterExtra:" << perf.pktRcvFilterExtra << sep;				// number of control packets received and not supplied back
        if (RCVR_O(level, 2)) ss << "pktRcvFilterSupply:" << perf.pktRcvFilterSupply << sep;			// number of packets that the filter supplied extra (e.g. FEC rebuilt)
        if (RCVR_O(level, 2)) ss << "pktRcvFilterLoss:" << perf.pktRcvFilterLoss << sep;				// number of packet loss not coverable by filter
        if (SNDR_O(level, 1)) ss << "mbpsSendRate:" << perf.mbpsSendRate << sep;						// sending rate in Mb/s
        if (RCVR_O(level, 1)) ss << "mbpsRecvRate:" << perf.mbpsRecvRate << sep;						// receiving rate in Mb/s
        if (SNDR_O(level, 1)) ss << "usSndDuration:" << perf.usSndDuration << sep;						// busy sending time (i.e., idle time exclusive)
        if (RCVR_O(level, 1)) ss << "pktReorderDistance:" << perf.pktReorderDistance << sep;			// size of order discrepancy in received sequences
        if (RCVR_O(level, 1)) ss << "pktReorderTolerance:" << perf.pktReorderTolerance << sep;			// packet reorder tolerance value
        if (RCVR_O(level, 1)) ss << "pktRcvAvgBelatedTime:" << perf.pktRcvAvgBelatedTime << sep;		// average time of packet delay for belated packets (packets with sequence past the ACK)
        if (RCVR_O(level, 1)) ss << "pktRcvBelated:" << perf.pktRcvBelated << sep;						// number of received AND IGNORED packets due to having come too late
        if (SNDR_O(level, 1)) ss << "pktSndDrop:" << perf.pktSndDrop << sep;							// number of too-late-to-send dropped packets
        if (RCVR_O(level, 1)) ss << "pktRcvDrop:" << perf.pktRcvDrop << sep;							// number of too-late-to play missing packets
        if (RCVR_O(level, 1)) ss << "pktRcvUndecrypt:" << perf.pktRcvUndecrypt << sep;					// number of undecrypted packets
        if (SNDR_O(level, 2)) ss << "byteSent:" << perf.byteSent << sep;								// number of sent data bytes, including retransmissions
        if (RCVR_O(level, 2)) ss << "byteRecv:" << perf.byteRecv << sep;								// number of received bytes
#if SRT_VERSION_VALUE >= SRT_MAKE_VERSION_VALUE(1,4,2)
        if (SNDR_O(level, 2)) ss << "byteSentUnique:" << perf.byteSentUnique << sep;					// number of data bytes, sent by the application
        if (RCVR_O(level, 2)) ss << "byteRecvUnique:" << perf.byteRecvUnique << sep;					// number of data bytes to be received by the application
#endif
        if (RCVR_O(level, 2)) ss << "byteRcvLoss:" << perf.byteRcvLoss << sep;							// number of retransmitted bytes
        if (SNDR_O(level, 2)) ss << "byteRetrans:" << perf.byteRetrans << sep;							// number of retransmitted bytes
        if (SNDR_O(level, 2)) ss << "byteSndDrop:" << perf.byteSndDrop << sep;							// number of too-late-to-send dropped bytes
        if (RCVR_O(level, 2)) ss << "byteRcvDrop:" << perf.byteRcvDrop << sep;							// number of too-late-to play missing bytes (estimate based on average packet size)
        if (RCVR_O(level, 2)) ss << "byteRcvUndecrypt:" << perf.byteRcvUndecrypt << sep;				// number of undecrypted bytes
                                                                                                        // instant measurements
        if (SNDR_O(level, 1)) ss << "usPktSndPeriod:" << perf.usPktSndPeriod << sep;					// packet sending period, in microseconds
        if (SNDR_O(level, 1)) ss << "pktFlowWindow:" << perf.pktFlowWindow << sep;						// flow window size, in number of packets
        if (SNDR_O(level, 1)) ss << "pktCongestionWindow:" << perf.pktCongestionWindow << sep;			// congestion window size, in number of packets
        if (SNDR_O(level, 1)) ss << "pktFlightSize:" << perf.pktFlightSize << sep;						// number of packets on flight
        if (COMMON(level, 1)) ss << "msRTT:" << perf.msRTT << sep;										// RTT, in milliseconds
#if SRT_VERSION_VALUE == SRT_MAKE_VERSION_VALUE(1,4,1)
        if (SNDR_O(level, 1)) ss << "mbpsBandwidth:" << perf.mbpsBandwidth << sep;						// estimated bandwidth, in Mb/s
#else
        if (COMMON(level, 1)) ss << "mbpsBandwidth:" << perf.mbpsBandwidth << sep;						// estimated bandwidth, in Mb/s
#endif
        if (SNDR_O(level, 1)) ss << "byteAvailSndBuf:" << perf.byteAvailSndBuf << sep;					// available UDT sender buffer size
        if (RCVR_O(level, 1)) ss << "byteAvailRcvBuf:" << perf.byteAvailRcvBuf << sep;					// available UDT receiver buffer size
        if (SNDR_O(level, 2)) ss << "mbpsMaxBW:" << perf.mbpsMaxBW << sep;								// Transmit Bandwidth ceiling (Mbps)
        if (COMMON(level, 2)) ss << "byteMSS:" << perf.byteMSS << sep;									// MTU
        if (SNDR_O(level, 1)) ss << "pktSndBuf:" << perf.pktSndBuf << sep;								// UnACKed packets in UDT sender
        if (SNDR_O(level, 2)) ss << "byteSndBuf:" << perf.byteSndBuf << sep;							// UnACKed bytes in UDT sender
        if (SNDR_O(level, 2)) ss << "msSndBuf:" << perf.msSndBuf << sep;								// UnACKed timespan (msec) of UDT sender
        if (SNDR_O(level, 2)) ss << "msSndTsbPdDelay:" << perf.msSndTsbPdDelay << sep;					// Timestamp-based Packet Delivery Delay
        if (RCVR_O(level, 1)) ss << "pktRcvBuf:" << perf.pktRcvBuf << sep;								// Undelivered packets in UDT receiver
        if (RCVR_O(level, 2)) ss << "byteRcvBuf:" << perf.byteRcvBuf << sep;							// Undelivered bytes of UDT receiver
        if (RCVR_O(level, 2)) ss << "msRcvBuf:" << perf.msRcvBuf << sep;								// Undelivered timespan (msec) of UDT receiver
        if (RCVR_O(level, 2)) ss << "msRcvTsbPdDelay:" << perf.msRcvTsbPdDelay << sep;					// Timestamp-based Packet Delivery Delay
#undef COMMON
#undef SNDR_O
#undef RCVR_O
        std::string s = ss.str();
        return s.substr(0, s.length() > sep.length() ? s.length() - sep.length() : std::string::npos);
    }
};

Sender::ptr_t Sender::Create(int sfd, const SendOption& option) {
    return ptr_t(new Sender(sfd, option));
}
Sender::Sender(int sfd, const SendOption& option)
    : pimpl_(new Impl(this, static_cast<SRTSOCKET>(sfd), option)) {
}
Sender::~Sender() {
    pimpl_.reset();
}
bool Sender::Initialize() {
    return pimpl_->Initialize();
}
void Sender::Destroy() {
    pimpl_->Destroy();
}
bool Sender::Send(const Event::buf_t& buf) {
    return pimpl_->Send(buf.data(), buf.size());
}
bool Sender::Send(const char* buf, size_t len) {
    return pimpl_->Send(buf, len);
}
const SendOption& Sender::GetOption() const {
    return pimpl_->GetOption();
}
std::string Sender::GetErrMsg(const std::string& sep) const {
    return pimpl_->GetErrMsg(sep);
}
std::string Sender::GetStatistics(int level, const std::string& sep) const {
    return pimpl_->GetStatistics(level, sep);
}
