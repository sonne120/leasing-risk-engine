#pragma once

#include "cache.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace leasing::common::ring {
template <typename T, std::size_t Capacity>
class MpmcQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");
    static_assert(Capacity >= 2, "Capacity must be at least 2");
    static_assert(std::is_default_constructible_v<T>,
                  "T must be default-constructible");

public:
    using value_type = T;

    MpmcQueue()
        : slots_(std::make_unique<Slot[]>(Capacity))
    {
        for (std::size_t i = 0; i < Capacity; ++i) {
            slots_[i].seq.store(i, std::memory_order_relaxed);
        }
        enq_pos_.store(0, std::memory_order_relaxed);
        deq_pos_.store(0, std::memory_order_relaxed);
    }

    MpmcQueue(const MpmcQueue&)            = delete;
    MpmcQueue& operator=(const MpmcQueue&) = delete;

    bool try_enqueue(T v) {
        Slot* slot;
        std::size_t pos = enq_pos_.load(std::memory_order_relaxed);
        for (;;) {
            slot = &slots_[pos & kMask];
            const std::size_t seq = slot->seq.load(std::memory_order_acquire);
            const auto diff =
                static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);
            if (diff == 0) {
                if (enq_pos_.compare_exchange_weak(
                        pos, pos + 1,
                        std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false;                       // full
            } else {
                pos = enq_pos_.load(std::memory_order_relaxed);
            }
        }
        slot->value = std::move(v);
        slot->seq.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool try_dequeue(T& out) {
        Slot* slot;
        std::size_t pos = deq_pos_.load(std::memory_order_relaxed);
        for (;;) {
            slot = &slots_[pos & kMask];
            const std::size_t seq = slot->seq.load(std::memory_order_acquire);
            const auto diff = static_cast<std::intptr_t>(seq) -
                              static_cast<std::intptr_t>(pos + 1);
            if (diff == 0) {
                if (deq_pos_.compare_exchange_weak(
                        pos, pos + 1,
                        std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false;                       // empty
            } else {
                pos = deq_pos_.load(std::memory_order_relaxed);
            }
        }
        out = std::move(slot->value);
        slot->seq.store(pos + Capacity, std::memory_order_release);
        return true;
    }

    // Approximate size and emptiness.
    std::size_t size_approx() const noexcept {
        const auto e = enq_pos_.load(std::memory_order_acquire);
        const auto d = deq_pos_.load(std::memory_order_acquire);
        return e > d ? e - d : 0;
    }
    bool empty_approx() const noexcept { return size_approx() == 0; }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    struct alignas(kCacheLine) Slot {
        std::atomic<std::size_t> seq;
        T                        value{};
    };

    std::unique_ptr<Slot[]> slots_;
    alignas(kCacheLine) std::atomic<std::size_t>    enq_pos_;
    alignas(kCacheLine) std::atomic<std::size_t>    deq_pos_;
};

} 
