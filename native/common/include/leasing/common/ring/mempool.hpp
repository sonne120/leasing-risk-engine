#pragma once

#include "cache.hpp"
#include "sequence.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace leasing::common::ring {
template <typename T, std::size_t Capacity>
class MemPool {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");

public:
    using value_type = T;

    MemPool() {
        storage_.resize(Capacity);
        // Prime the free list with pointers to every slot.
        for (std::size_t i = 0; i < Capacity; ++i) {
            free_slots_[i].store(&storage_[i], std::memory_order_relaxed);
        }
        head_.store(Capacity, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    MemPool(const MemPool&)            = delete;
    MemPool& operator=(const MemPool&) = delete;

    // Returns nullptr if the pool is empty (all slots in use).
    T* acquire() noexcept {
        std::size_t t = tail_.load(std::memory_order_relaxed);
        for (;;) {
            const std::size_t h = head_.load(std::memory_order_acquire);
            if (t >= h) {
                return nullptr;             // empty
            }
            T* p = free_slots_[t & kMask].load(std::memory_order_acquire);
            if (!p) {
                // Producer has moved head but not yet published the pointer.
                continue;
            }
            if (tail_.compare_exchange_weak(
                    t, t + 1,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed)) {
                // We won the slot.
                free_slots_[t & kMask].store(nullptr, std::memory_order_release);
                return p;
            }
            // t was updated by CAS failure — loop and retry.
        }
    }

    // Return a buffer to the pool.
    void release(T* p) noexcept {
        std::size_t h = head_.load(std::memory_order_relaxed);
        for (;;) {
            // Publish the pointer first, then advance head.
            T* expected = nullptr;
            if (!free_slots_[h & kMask].compare_exchange_strong(
                    expected, p,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed)) {
                // Slot not yet cleared by an acquire — spin briefly.
                h = head_.load(std::memory_order_relaxed);
                continue;
            }
            if (head_.compare_exchange_weak(
                    h, h + 1,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed)) {
                return;
            }
            // Someone else moved head — unpublish and retry.
            free_slots_[h & kMask].store(nullptr, std::memory_order_release);
        }
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

    std::size_t in_use_approx() const noexcept {
        const std::size_t h = head_.load(std::memory_order_acquire);
        const std::size_t t = tail_.load(std::memory_order_acquire);
        return Capacity - (h - t);
    }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    std::vector<T> storage_;
    std::array<std::atomic<T*>, Capacity> free_slots_{};

    alignas(kCacheLine) std::atomic<std::size_t> head_{0};
    alignas(kCacheLine) std::atomic<std::size_t> tail_{0};
};

} 
