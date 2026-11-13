#pragma once

#include "sequence.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

namespace leasing::common::ring {
template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");
    static_assert(Capacity >= 2, "Capacity must be at least 2");

public:
    using value_type = T;

    SpscQueue() noexcept = default;

    SpscQueue(const SpscQueue&)            = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;

    // Producer side.
    bool try_push(const T& v) {
        const std::int64_t h = head_.get_relaxed();
        const std::int64_t t = tail_.get_acquire();
        if (h - t >= static_cast<std::int64_t>(Capacity)) {
            return false;                       // full
        }
        slots_[static_cast<std::size_t>(h) & kMask] = v;
        head_.set_release(h + 1);               // publish
        return true;
    }

    bool try_push(T&& v) {
        const std::int64_t h = head_.get_relaxed();
        const std::int64_t t = tail_.get_acquire();
        if (h - t >= static_cast<std::int64_t>(Capacity)) {
            return false;
        }
        slots_[static_cast<std::size_t>(h) & kMask] = std::move(v);
        head_.set_release(h + 1);
        return true;
    }

    // Consumer side.
    std::optional<T> try_pop() {
        const std::int64_t t = tail_.get_relaxed();
        const std::int64_t h = head_.get_acquire();
        if (t >= h) {
            return std::nullopt;                // empty
        }
        T v = std::move(slots_[static_cast<std::size_t>(t) & kMask]);
        tail_.set_release(t + 1);
        return v;
    }

    // Observers.
    std::size_t size_approx() const noexcept {
        const std::int64_t h = head_.get_acquire();
        const std::int64_t t = tail_.get_acquire();
        return static_cast<std::size_t>(h - t);
    }

    bool empty_approx() const noexcept { return size_approx() == 0; }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    alignas(kCacheLine) Sequence head_{0};
    alignas(kCacheLine) Sequence tail_{0};

    // Slots.
    alignas(kCacheLine) std::array<T, Capacity> slots_{};
};

} 
