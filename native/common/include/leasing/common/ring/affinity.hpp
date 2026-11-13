#pragma once

#include <thread>

#if defined(__linux__)
  #ifndef _GNU_SOURCE
    #define _GNU_SOURCE
  #endif
  #include <pthread.h>
  #include <sched.h>
#elif defined(__APPLE__)
  #include <mach/mach.h>
  #include <mach/thread_act.h>
  #include <mach/thread_policy.h>
  #include <pthread.h>
#elif defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif

namespace leasing::common::ring {
// Pin the CURRENT thread to a single logical CPU.
inline bool pin_this_thread(int cpu) {
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
#elif defined(__APPLE__)
    thread_affinity_policy_data_t policy{ cpu + 1 };    // tag 0 = no hint
    mach_port_t mt = pthread_mach_thread_np(pthread_self());
    return thread_policy_set(mt, THREAD_AFFINITY_POLICY,
                             reinterpret_cast<thread_policy_t>(&policy),
                             THREAD_AFFINITY_POLICY_COUNT) == KERN_SUCCESS;
#elif defined(_WIN32)
    DWORD_PTR mask = static_cast<DWORD_PTR>(1) << cpu;
    return SetThreadAffinityMask(GetCurrentThread(), mask) != 0;
#else
    (void)cpu;
    return false;
#endif
}

// Number of logical CPUs available to this process.
inline unsigned cpu_count() noexcept {
    const unsigned n = std::thread::hardware_concurrency();
    return n == 0 ? 1u : n;
}

} 
