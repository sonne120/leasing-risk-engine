#pragma once

#include "Snapshot.h"
#include <functional>

namespace leasing::pipeline {
class ProgressBus {
public:
    // std::function form — the clean path for native callers.
    using Callback = std::function<void(const Snapshot&)>;

    // Raw C-style function pointer with opaque user data.
    using RawCallback = void (*)(const Snapshot*, void*);

    void set_callback(Callback cb) {
        cb_ = std::move(cb);
        raw_ = nullptr;
        user_data_ = nullptr;
    }

    void set_raw_callback(RawCallback cb, void* user_data) {
        raw_ = cb;
        user_data_ = user_data;
        cb_ = nullptr;
    }

    bool has_callback() const noexcept {
        return static_cast<bool>(cb_) || raw_ != nullptr;
    }

    void publish(const Snapshot& s) const {
        if (cb_) {
            cb_(s);
        } else if (raw_) {
            raw_(&s, user_data_);
        }
    }

private:
    Callback    cb_;
    RawCallback raw_      = nullptr;
    void*       user_data_= nullptr;
};

} 
