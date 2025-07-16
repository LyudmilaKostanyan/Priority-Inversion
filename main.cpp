#include <iostream>
#include <thread>
#include <chrono>

#ifdef _WIN32
	#include <windows.h>
#else
	#include <pthread.h>
#endif

// Set thread priority: 0=low, 1=medium, 2=high
void set_priority(std::thread& t, int level) {
#ifdef _WIN32
	int prio = THREAD_PRIORITY_NORMAL;
	if (level == 0) prio = THREAD_PRIORITY_LOWEST;
	if (level == 1) prio = THREAD_PRIORITY_ABOVE_NORMAL;
	if (level == 2) prio = THREAD_PRIORITY_HIGHEST;
	SetThreadPriority(t.native_handle(), prio);
#else
	sched_param sch;
	int policy = SCHED_FIFO;
	int max = sched_get_priority_max(policy);
	int min = sched_get_priority_min(policy);
	int prio = min + (max - min) * level / 2; // level 0..2 → min..max
	sch.sched_priority = prio;
	pthread_setschedparam(t.native_handle(), policy, &sch);
#endif
}

void low_task() {
	for (int i = 0; i < 5; ++i) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		std::cout << "[Low] log sensor data\n";
	}
}

void medium_task() {
	for (int i = 0; i < 5; ++i) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		std::cout << "[Medium] process communication\n";
	}
}

void high_task() {
	for (int i = 0; i < 5; ++i) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		std::cout << "[High] execute critical command\n";
	}
}

int main() {
	std::cout << "=== Fixed-Priority Scheduling (no resource conflict) ===\n";

	std::thread low(low_task);
	std::thread medium(medium_task);
	std::thread high(high_task);

	set_priority(low, 0);    // Low
	set_priority(medium, 1); // Medium
	set_priority(high, 2);   // High

	high.join();
	medium.join();
	low.join();

	std::cout << "=== Done ===\n";
	return 0;
}
