#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

std::atomic<bool> done{false};

#ifndef _WIN32
pthread_mutex_t pi_mutex;
#endif

// Set thread priority: 0=low, 1=medium, 2=high
void set_priority(std::thread& t, int level) {
#ifdef _WIN32
    int prio = THREAD_PRIORITY_NORMAL;
    if (level == 0) prio = THREAD_PRIORITY_LOWEST;
    else if (level == 1) prio = THREAD_PRIORITY_NORMAL;
    else if (level == 2) prio = THREAD_PRIORITY_HIGHEST;
    SetThreadPriority(t.native_handle(), prio);
#else
    sched_param sch;
    int policy = SCHED_FIFO;
    int max = sched_get_priority_max(policy);
    int min = sched_get_priority_min(policy);
    int prio = min + (max - min) * level / 2;
    sch.sched_priority = prio;
    pthread_setschedparam(t.native_handle(), policy, &sch);
#endif
}

// Low-priority thread: acquires the shared resource (mutex) and holds it for a while
void low_task() {
#ifndef _WIN32
    pthread_mutex_lock(&pi_mutex);
#else
    // On Windows, Priority Inheritance is not natively supported for mutexes
    // For demonstration only; not effective for real inversion resolution
    // std::lock_guard<std::mutex> lock(resource_mutex);
#endif
    std::cout << "[Low] Acquired resource\n";
    // Simulate a long operation while holding the resource
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "[Low] Releasing resource\n";
#ifndef _WIN32
    pthread_mutex_unlock(&pi_mutex);
#endif
    std::cout << "[Low] Released resource\n";
}

// Medium-priority thread: continuously consumes CPU, preventing low-priority thread from running
void medium_task() {
    while (!done) {
        // Busy work: keeps the CPU occupied
    }
    std::cout << "[Medium] Finished\n";
}

// High-priority thread: needs the shared resource for critical work
void high_task() {
    // Give the low-priority thread a head start to acquire the resource first
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "[High] Trying to acquire resource...\n";
#ifndef _WIN32
    pthread_mutex_lock(&pi_mutex);
#else
    // std::lock_guard<std::mutex> lock(resource_mutex);
#endif
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "[High] Acquired resource\n";
#ifndef _WIN32
    pthread_mutex_unlock(&pi_mutex);
#endif
    std::chrono::duration<double, std::milli> wait_time = end - start;
    std::cout << "[High] Waited " << wait_time.count() << " ms\n";
    done = true; // Notify medium-priority thread to stop
}

int main() {
    std::cout << "=== Mars Pathfinder Priority Inversion (Priority Inheritance) ===\n";

#ifndef _WIN32
    // Initialize a mutex with Priority Inheritance protocol
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
    pthread_mutex_init(&pi_mutex, &attr);
#endif

    // Launch threads for each role
    std::thread low(low_task);
    set_priority(low, 0); // Low priority

    std::thread medium(medium_task);
    set_priority(medium, 1); // Medium priority

    std::thread high(high_task);
    set_priority(high, 2); // High priority

    high.join();
    medium.join();
    low.join();

#ifndef _WIN32
    pthread_mutex_destroy(&pi_mutex);
#endif

    std::cout << "=== Done ===\n";
    return 0;
}
