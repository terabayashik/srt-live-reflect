#include "stdafx.h"
#include "event.h"

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
void Event::AddEvent(boost::mutex& mutex, list_t& list, pair_t pair) {
    boost::mutex::scoped_lock lk(mutex);
    list.insert(std::lower_bound(list.begin(), list.end(), pair, [](const pair_t& lhs, const pair_t& rhs) {
        return !(lhs.first - rhs.first < 0); // sort descending by priority
    }), pair);
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
Event::vector_t Event::GetEvents(boost::mutex& mutex, list_t& list) {
    boost::mutex::scoped_lock lk(mutex);
    vector_t vector;
    for (list_t::iterator it = list.begin(); it != list.end(); ) {
        ptr_t ptr = it->second.lock();
        if (ptr) {
            vector.push_back(ptr);
            ++it;
        } else {
            it = list.erase(it);
        }
    }
    return vector;
}
