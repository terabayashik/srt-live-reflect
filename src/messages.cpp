#include "stdafx.h"
#include "messages.h"

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
Messages& Messages::operator<<(const std::string& str) {
    if (!str.empty()) {
        msgs_.push_back(str);
        while (msgs_.size() > keep_) {
            msgs_.pop_front();
        }
    }
    return *this;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
std::string Messages::operator()(const std::string& sep) const {
    std::string msgs;
    for (msgs_t::const_iterator it = msgs_.begin(); it != msgs_.end(); ++it) {
        if (!msgs.empty()) msgs += sep;
        msgs += *it;
    }
    return msgs;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
Messages& Messages::Clear::operator()(Messages& msgs) {
    msgs.msgs_.clear();
    return msgs;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
Messages& Messages::Resize::operator()(Messages& msgs) {
    msgs.keep_ = resize_;
    while (msgs.msgs_.size() > msgs.keep_) {
        msgs.msgs_.pop_front();
    }
    return msgs;
}

