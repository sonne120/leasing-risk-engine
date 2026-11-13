#include <leasing/common/ring/cache.hpp>
#include <leasing/common/ring/sequence.hpp>
#include <leasing/common/ring/spsc_queue.hpp>
#include <leasing/common/ring/mempool.hpp>
#include <leasing/common/ring/affinity.hpp>
#include <leasing/common/ring/timing.hpp>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

using namespace leasing::common::ring;

namespace {
int    g_passed = 0;
int    g_failed = 0;

#define REQUIRE(cond)                                                           \
    do {                                                                        \
        if (cond) {                                                             \
            ++g_passed;                                                         \
        } else {                                                                \
            ++g_failed;                                                         \
            std::fprintf(stderr,                                                \
                         "  FAIL  %s:%d  %s\n", __FILE__, __LINE__, #cond);     \
        }                                                                       \
    } while (0)

void test_cache_line_padding() {
    std::printf("test_cache_line_padding\n");
    REQUIRE(kCacheLine == 128);
    REQUIRE(sizeof(Sequence) == kCacheLine);
    REQUIRE(alignof(Sequence) == kCacheLine);

    // Two padded sequences next to each other must sit on different lines.
    struct Pair {
        Sequence a;
        Sequence b;
    };
    Pair p;
    const auto addr_a = reinterpret_cast<std::uintptr_t>(&p.a);
    const auto addr_b = reinterpret_cast<std::uintptr_t>(&p.b);
    REQUIRE(addr_b - addr_a >= kCacheLine);
}

void test_sequence_cas() {
    std::printf("test_sequence_cas\n");
    Sequence s{10};
    REQUIRE(s.get_relaxed() == 10);
    REQUIRE(s.compare_and_set(10, 11));
    REQUIRE(s.get_relaxed() == 11);
    REQUIRE(!s.compare_and_set(10, 12));          // stale expected
    REQUIRE(s.get_relaxed() == 11);
    REQUIRE(s.fetch_add(5) == 11);
    REQUIRE(s.get_relaxed() == 16);
}

void test_spsc_single_threaded() {
    std::printf("test_spsc_single_threaded\n");
    SpscQueue<int, 4> q;
    REQUIRE(q.empty_approx());
    REQUIRE(q.try_push(1));
    REQUIRE(q.try_push(2));
    REQUIRE(q.try_push(3));
    REQUIRE(q.try_push(4));
    REQUIRE(!q.try_push(5));                       // full
    REQUIRE(q.size_approx() == 4);

    REQUIRE(q.try_pop().value() == 1);
    REQUIRE(q.try_pop().value() == 2);
    REQUIRE(q.try_push(5));                        // room again
    REQUIRE(q.try_push(6));
    REQUIRE(q.try_pop().value() == 3);
    REQUIRE(q.try_pop().value() == 4);
    REQUIRE(q.try_pop().value() == 5);
    REQUIRE(q.try_pop().value() == 6);
    REQUIRE(!q.try_pop().has_value());
}

void test_spsc_concurrent() {
    std::printf("test_spsc_concurrent\n");
    constexpr int   N  = 200'000;
    SpscQueue<int, 1024> q;

    std::atomic<bool> ready{false};
    std::uint64_t     checksum_out = 0;

    std::thread producer([&] {
        while (!ready.load(std::memory_order_acquire)) { /* spin */ }
        for (int i = 0; i < N; ++i) {
            while (!q.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        while (!ready.load(std::memory_order_acquire)) { /* spin */ }
        std::uint64_t sum = 0;
        int           got = 0;
        while (got < N) {
            auto v = q.try_pop();
            if (v) {
                sum += static_cast<std::uint64_t>(*v);
                ++got;
            } else {
                std::this_thread::yield();
            }
        }
        checksum_out = sum;
    });

    ready.store(true, std::memory_order_release);
    producer.join();
    consumer.join();

    const std::uint64_t expected =
        static_cast<std::uint64_t>(N) * (N - 1) / 2;
    REQUIRE(checksum_out == expected);
}

void test_mempool_acquire_release() {
    std::printf("test_mempool_acquire_release\n");
    MemPool<std::vector<double>, 8> pool;
    REQUIRE(pool.in_use_approx() == 0);

    auto* a = pool.acquire();
    auto* b = pool.acquire();
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(a != b);
    REQUIRE(pool.in_use_approx() == 2);

    pool.release(a);
    pool.release(b);
    REQUIRE(pool.in_use_approx() == 0);

    // Exhaust the pool.
    std::vector<std::vector<double>*> taken;
    for (std::size_t i = 0; i < pool.capacity(); ++i) {
        auto* p = pool.acquire();
        REQUIRE(p != nullptr);
        taken.push_back(p);
    }
    REQUIRE(pool.acquire() == nullptr);            // exhausted
    for (auto* p : taken) pool.release(p);
    REQUIRE(pool.in_use_approx() == 0);
}

void test_timing_and_affinity() {
    std::printf("test_timing_and_affinity\n");
    StopWatch sw;
    volatile std::uint64_t sink = 0;
    for (int i = 0; i < 100'000; ++i) sink += i;
    const double ms = sw.elapsed_ms();
    REQUIRE(ms >= 0.0);
    REQUIRE(sink > 0);                             // keep the loop

    // Pin to CPU 0 — may fail on sandboxed CI, non-fatal.
    const bool pinned = pin_this_thread(0);
    (void)pinned;
    REQUIRE(cpu_count() >= 1);
}

} 

int main() {
    test_cache_line_padding();
    test_sequence_cas();
    test_spsc_single_threaded();
    test_spsc_concurrent();
    test_mempool_acquire_release();
    test_timing_and_affinity();

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
