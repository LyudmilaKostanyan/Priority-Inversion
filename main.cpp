#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>

std::mutex resource_mutex;
std::atomic<bool> low_done{false};
std::atomic<bool> high_waiting{false};

// Simulate low-priority thread
void low_priority_task(bool with_inheritance) {
	std::cout << "[Low] Starting and locking resource\n";
	resource_mutex.lock();
	std::this_thread::sleep_for(std::chrono::milliseconds(100)); // simulate work

	while (high_waiting && !with_inheritance) {
		// low can't run because medium is preempting it
		std::this_thread::yield(); // simulate being preempted
	}

	std::cout << "[Low] Unlocking resource\n";
	resource_mutex.unlock();
	low_done = true;
}

// Simulate medium-priority thread (keeps CPU busy)
void medium_priority_task() {
	std::cout << "[Medium] Starting\n";
	while (!low_done) {
		for (volatile int i = 0; i < 10000; ++i) {}
		std::this_thread::yield();
	}
	std::cout << "[Medium] Finished\n";
}

// Simulate high-priority thread (needs resource)
void high_priority_task() {
	std::this_thread::sleep_for(std::chrono::milliseconds(10)); // ensure low locks first
	std::cout << "[High] Trying to lock resource\n";
	auto start = std::chrono::high_resolution_clock::now();

	high_waiting = true;
	resource_mutex.lock(); // will block if low still holds the mutex
	auto end = std::chrono::high_resolution_clock::now();

	std::cout << "[High] Acquired resource, doing work\n";
	resource_mutex.unlock();
	high_waiting = false;

	double wait_time = std::chrono::duration<double, std::milli>(end - start).count();
	std::cout << "[High] Waited " << wait_time << " ms\n";
}

void run_simulation(bool with_inheritance) {
	std::cout << "\n=== Simulation "
			<< (with_inheritance ? "WITH" : "WITHOUT") << " Priority Inheritance ===\n";

	low_done = false;
	high_waiting = false;

	std::thread low([&]() { low_priority_task(with_inheritance); });
	std::thread medium(medium_priority_task);
	std::thread high(high_priority_task);

	high.join();
	medium.join();
	low.join();
}

int main() {
	std::cout << "Choose mode:\n"
				<< "1 = Run simulation without priority inheritance\n"
				<< "2 = Run simulation with priority inheritance\n"
				<< "Enter your choice: ";
	int choice;
	std::cin >> choice;
	switch (choice)
	{
		case 1:
			std::cout << "Running simulation without priority inheritance...\n";
			run_simulation(false);
			break;
		case 2:
			std::cout << "Running simulation with priority inheritance...\n";
			run_simulation(true);
			break;
		default:
			std::cout << "Invalid choice. Exiting.\n";
			return 1;
	}
	return 0;
}
