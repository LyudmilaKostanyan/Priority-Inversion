#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

std::atomic<bool> done{false};
std::atomic<bool> high_waiting{false};
std::atomic<bool> low_started{false};

#ifdef _WIN32
std::mutex resource_mutex;
HANDLE low_thread_handle = nullptr;
int low_original_priority = THREAD_PRIORITY_LOWEST;
#endif

#ifndef _WIN32
pthread_mutex_t pi_mutex;
pthread_mutex_t plain_mutex;
#endif

enum Mode {
    INHERITANCE,
    NO_INHERITANCE
};
Mode mode = INHERITANCE;

// Thread priority utility (same for both modes)
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

// Low-priority thread
void low_task() {
#ifdef _WIN32
    low_thread_handle = GetCurrentThread();
    low_original_priority = GetThreadPriority(low_thread_handle);
    if (mode == INHERITANCE) {
        resource_mutex.lock();
        std::cout << "[Low] Acquired resource\n";
        int sleep_count = 30;
        for (int i = 0; i < sleep_count; ++i) {
            if (high_waiting && GetThreadPriority(low_thread_handle) != THREAD_PRIORITY_HIGHEST) {
                SetThreadPriority(low_thread_handle, THREAD_PRIORITY_HIGHEST);
                std::cout << "[Low] Priority temporarily raised to HIGH\n";
            }
            if (!high_waiting && GetThreadPriority(low_thread_handle) != low_original_priority) {
                SetThreadPriority(low_thread_handle, low_original_priority);
                std::cout << "[Low] Priority restored to LOW\n";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        SetThreadPriority(low_thread_handle, low_original_priority);
        std::cout << "[Low] Releasing resource\n";
        resource_mutex.unlock();
        std::cout << "[Low] Released resource\n";
    } else { // NO_INHERITANCE
        resource_mutex.lock();
        std::cout << "[Low] Acquired resource\n";
        low_started = true;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::cout << "[Low] Releasing resource\n";
        resource_mutex.unlock();
        std::cout << "[Low] Released resource\n";
    }
#else
    if (mode == INHERITANCE) {
        pthread_mutex_lock(&pi_mutex);
        std::cout << "[Low] Acquired resource\n";
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::cout << "[Low] Releasing resource\n";
        pthread_mutex_unlock(&pi_mutex);
        std::cout << "[Low] Released resource\n";
    } else { // NO_INHERITANCE
        pthread_mutex_lock(&plain_mutex);
        std::cout << "[Low] Acquired resource\n";
        low_started = true;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::cout << "[Low] Releasing resource\n";
        pthread_mutex_unlock(&plain_mutex);
        std::cout << "[Low] Released resource\n";
    }
#endif
}

// Aggressive CPU hog (medium)
void medium_task() {
    if (mode == NO_INHERITANCE) {
        // Wait until low acquired the mutex, then aggressively hog CPU
        while (!low_started)
            ;
        std::cout << "[Medium] Heavy CPU load starts\n";
        auto start = std::chrono::steady_clock::now();
        while (!done) {
            volatile double x = 1.0;
            for (int i = 0; i < 1000000; ++i)
                x = x * 1.0000001 + 0.0000001;
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(10)) {
                std::cout << "[Medium] Still hogging CPU...\n";
                start = std::chrono::steady_clock::now();
            }
        }
        std::cout << "[Medium] Finished\n";
    } else {
        // Classic busy wait (less aggressive, since inheritance resolves issue)
        while (!done) {
            // Simple busy loop
        }
        std::cout << "[Medium] Finished\n";
    }
}

// High-priority thread: wants the resource, blocks until it's free
void high_task() {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "[High] Trying to acquire resource...\n";
#ifdef _WIN32
    if (mode == INHERITANCE) {
        high_waiting = true;
        resource_mutex.lock();
        auto end = std::chrono::high_resolution_clock::now();
        high_waiting = false;
        std::cout << "[High] Acquired resource\n";
        resource_mutex.unlock();
        std::chrono::duration<double, std::milli> wait_time = end - start;
        std::cout << "[High] Waited " << wait_time.count() << " ms\n";
    } else { // NO_INHERITANCE
        resource_mutex.lock();
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "[High] Acquired resource\n";
        resource_mutex.unlock();
        std::chrono::duration<double, std::milli> wait_time = end - start;
        std::cout << "[High] Waited " << wait_time.count() << " ms\n";
    }
#else
    if (mode == INHERITANCE) {
        pthread_mutex_lock(&pi_mutex);
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "[High] Acquired resource\n";
        pthread_mutex_unlock(&pi_mutex);
        std::chrono::duration<double, std::milli> wait_time = end - start;
        std::cout << "[High] Waited " << wait_time.count() << " ms\n";
    } else { // NO_INHERITANCE
        pthread_mutex_lock(&plain_mutex);
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "[High] Acquired resource\n";
        pthread_mutex_unlock(&plain_mutex);
        std::chrono::duration<double, std::milli> wait_time = end - start;
        std::cout << "[High] Waited " << wait_time.count() << " ms\n";
    }
#endif
    done = true; // Notify medium to stop
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [--inheritance | --no-inheritance]\n"
              << "  --inheritance     Simulate priority inheritance (default)\n"
              << "  --no-inheritance  Simulate priority inversion without inheritance (Mars Pathfinder bug)\n";
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        if (strcmp(argv[1], "--inheritance") == 0)
            mode = INHERITANCE;
        else if (strcmp(argv[1], "--no-inheritance") == 0)
            mode = NO_INHERITANCE;
        else {
            print_usage(argv[0]);
            return 1;
        }
    }

    std::cout << "=== Mars Pathfinder Priority Inversion Simulation ===\n";
    if (mode == NO_INHERITANCE)
        std::cout << "(Simulating WITHOUT priority inheritance: possible deadlock/long wait)\n";
    else
        std::cout << "(Simulating WITH priority inheritance: bug avoided)\n";

#ifndef _WIN32
    // Prepare mutexes
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
    pthread_mutex_init(&pi_mutex, &attr);
    pthread_mutex_init(&plain_mutex, nullptr);
#endif

    std::thread low(low_task);
    set_priority(low, 0);

    std::thread medium(medium_task);
    set_priority(medium, 1);

    std::thread high(high_task);
    set_priority(high, 2);

    high.join();
    medium.join();
    low.join();

#ifndef _WIN32
    pthread_mutex_destroy(&pi_mutex);
    pthread_mutex_destroy(&plain_mutex);
#endif

    std::cout << "=== Done ===\n";
    return 0;
}
