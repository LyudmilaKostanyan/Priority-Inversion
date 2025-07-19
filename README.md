# Mars Pathfinder Priority Inversion Simulation

## Table of Contents

1. [Overview](#overview)
2. [The Story: Mars Pathfinder and the Real-World Consequences of Concurrency Bugs](#the-story-mars-pathfinder-and-the-real-world-consequences-of-concurrency-bugs)

   * [Mars Pathfinder Hardware and Scheduling Constraints](#mars-pathfinder-hardware-and-scheduling-constraints)
3. [What Does This Project Demonstrate?](#what-does-this-project-demonstrate)
4. [Example Output](#example-output)
5. [Explanation of Output](#explanation-of-output)
6. [Why This Cannot Be Fully Reproduced on macOS](#why-this-cannot-be-fully-reproduced-on-macos)
7. [Limitations of CI Environments and Windows](#limitations-of-ci-environments-and-windows)
8. [How to Compile and Run](#how-to-compile-and-run)

   * [Linux](#linux)
   * [Windows](#windows)
   * [macOS](#macos)
9. [Parameters](#parameters)

## Overview

This project is a detailed simulation of the **priority inversion bug** that famously occurred on NASA’s Mars Pathfinder mission in 1997, and a demonstration of how modern operating systems resolve such concurrency bugs through **priority inheritance**.

---

### The Story: Mars Pathfinder and the Real-World Consequences of Concurrency Bugs

In July 1997, NASA’s Mars Pathfinder successfully landed on Mars and began its mission to explore the Martian surface. However, soon after landing, mission control noticed that the onboard computer would occasionally **reset itself unexpectedly**, causing loss of science data and halting operations for hours.

After intense investigation, engineers discovered a classic **priority inversion** in the rover’s real-time operating system (VxWorks):

* The rover software had several threads, each assigned a fixed priority.

  * The **high-priority thread** handled critical communications with Earth.
  * The **low-priority thread** performed background logging and held a lock (mutex) to a shared resource.
  * The **medium-priority thread** performed routine data processing and could consume significant CPU.
* The low-priority thread acquired a mutex and was preempted before releasing it.
* The high-priority thread needed the same mutex and became blocked, waiting.
* The medium-priority thread, not needing the mutex, would continuously preempt the low-priority thread, preventing it from releasing the mutex.
* The high-priority thread was **starved**—it could not proceed until the low-priority thread ran and released the mutex.
* Because the high-priority task could not run, the watchdog timer would trigger a system reset, causing a loss of data and missed real-time deadlines.

**This is called priority inversion:** the most important task is “inverted” to the lowest effective priority because of resource contention and unfortunate scheduling.

The fix was to add **priority inheritance**: when a high-priority thread is blocked waiting for a lock held by a lower-priority thread, the low-priority thread is temporarily *boosted* to the high priority so it can release the resource quickly.

#### Mars Pathfinder Hardware and Scheduling Constraints

The Mars Pathfinder lander used a single-core, single-threaded IBM RISC 601 processor running at 20 MHz, with only 128 MB of RAM and a real-time operating system (VxWorks). All onboard software tasks had to share this single CPU, and there was no hardware support for simultaneous multi-threading or multiple cores. As a result, if a lower-priority task held a resource needed by a higher-priority task, and a medium-priority task kept the CPU busy, the high-priority task could be blocked for long periods—leading to missed deadlines, system resets, and loss of scientific data. This unique combination of limited hardware and real-time constraints made the priority inversion bug especially dangerous on Mars Pathfinder.

---

## What Does This Project Demonstrate?

* **Reproduce the Mars Pathfinder bug:**
  The simulation creates a scenario where a low-priority thread acquires a shared resource (mutex), a high-priority thread needs this resource for critical work, and a CPU-intensive medium-priority thread keeps the CPU busy. The medium thread prevents the low-priority thread from running, so the high-priority thread is starved—just like the real Mars Pathfinder case.
* **See the effect of CPU affinity:**
  Running all threads on a single CPU core makes the inversion effect much more severe and brings your desktop simulation as close as possible to the real Mars Pathfinder bug. On Linux, use `taskset -c 0 ./main ...` to force this. On Windows (locally, not in CI), use `start /wait /affinity 1 ....`
Why is this important?
On a typical modern desktop or laptop with multiple CPU cores, even if one thread is busy, the operating system’s scheduler can run other threads on other cores. This means that, even in the presence of a buggy priority inversion scenario, the low-priority thread will eventually get scheduled on a free core and release the resource, so your simulation will "finish" after a short delay rather than hanging or creating a catastrophic delay.
However, on the real Mars Pathfinder, the hardware was a single-core, single-threaded processor (an IBM RISC 601 CPU running at 20 MHz with only 128MB RAM and very limited multitasking ability). Under these real-time conditions, with all tasks forced to compete for a single CPU, the priority inversion could completely prevent the high-priority task from running.
Therefore, forcing all threads onto a single core in your simulation reproduces the real bottleneck: the medium-priority thread can "hog" the only available CPU, indefinitely delaying the low-priority thread, and thus blocking the high-priority thread from ever making progress—exactly as happened on Mars.
This is why you should always run the simulation with CPU affinity enabled (single core), especially when testing without priority inheritance.
* **Observe the fix:**
  With priority inheritance enabled, if the high-priority thread is blocked by the low-priority thread, the program temporarily boosts the low-priority thread’s priority. This allows it to preempt the medium thread and release the mutex quickly, solving the bug.
* **Implementation details:**

  * Written in portable C++ with `std::thread` and mutexes, plus platform-specific logic for setting thread priorities and affinity.
  * On Linux, uses a POSIX mutex with `PTHREAD_PRIO_INHERIT` if available for a realistic kernel-level effect.
  * On Windows, the simulation manually boosts low-priority thread priority if the high-priority thread is blocked.
  * The medium thread runs a busy-wait loop to simulate heavy CPU contention.

---

## Example Output

### 1. No special settings (default, multi-core)

```
=== Mars Pathfinder Priority Inversion Simulation ===
(Simulating WITHOUT priority inheritance: possible deadlock/long wait)
[Low] Acquired resource
[Medium] Heavy CPU load starts
[High] Trying to acquire resource...
[Low] Releasing resource
[Low] Released resource
[High] Acquired resource
[High] Waited 2800.03 ms
[Medium] Finished
=== Done ===
```

---

### 2. With single-core pinning/affinity (`taskset`):

```
=== Mars Pathfinder Priority Inversion Simulation ===
(Simulating WITHOUT priority inheritance: possible deadlock/long wait)
[Low] Acquired resource
[Medium] Heavy CPU load starts
[High] Trying to acquire resource...
[Medium] Still hogging CPU...
[Medium] Still hogging CPU...
...
```

---

### 3. With priority inheritance enabled (on a single core):

```
=== Mars Pathfinder Priority Inversion Simulation ===
(Simulating WITH priority inheritance: bug avoided)
[Low] Acquired resource
[High] Trying to acquire resource...
[Low] Priority temporarily raised to HIGH
[Low] Releasing resource
[Low] Released resource
[High] Acquired resource
[High] Waited 3110.4 ms
[Medium] Finished
=== Done ===
```

---

## Explanation of Output

* **Waited N ms:** Time the high-priority thread was blocked. In the real bug, this could be nearly indefinite; in simulation, with affinity and CPU load, it can be very long.
* **\[Medium] Still hogging CPU...**: The medium-priority thread monopolizes the CPU, preventing progress.
* **\[Low] Priority temporarily raised to HIGH:** Priority inheritance is being simulated—the low-priority thread is boosted to release the mutex.

---

## Why This Cannot Be Fully Reproduced on macOS

* **macOS does not provide a standard tool for core pinning**—there is no equivalent to `taskset` or `/affinity`.
* The macOS scheduler is designed for “fairness”; every thread eventually gets CPU time, so real deadlocks do not occur as on a real-time OS.
* As a result, you will see delays, but not indefinite blocking as on VxWorks or with strict single-core affinity on Linux/Windows.
* Tools like `cpulimit` only restrict total CPU usage, not core affinity.

---

## Limitations of CI Environments and Windows

> **Note:**
> In cloud CI environments such as GitHub Actions, Windows runners cannot reliably simulate single-core execution due to virtualization and cloud CPU scheduling. Affinity and thread priority settings are often ignored, and resource sharing with other virtual machines can make thread scheduling unpredictable.
>
> For the most reliable demonstration and analysis, prefer local testing on Linux with `taskset`, or on your own Windows machine if you can set affinity manually.

---

## How to Compile and Run

### 1. Clone the Repository

```bash
git clone https://github.com/LyudmilaKostanyan/Priority-Inversion.git
cd Priority-Inversion
```

### 2. Build the Project

Requires CMake 3.16+ and a C++ compiler (gcc/g++ or clang/clang++).

```bash
cmake -S . -B build
cmake --build build
```

### 3. Run the Program

#### Linux

* **To simulate the Mars bug (without priority inheritance):**

  ```bash
  # Strongly recommended: use taskset to pin to a single core!
  taskset -c 0 ./build/main --no-inheritance
  # If the program hangs or takes too long, stop it manually with Ctrl+C.
  ```
* **With priority inheritance (default/fixed):**

  ```bash
  taskset -c 0 ./build/main
  # Press Ctrl+C to stop the program if needed.
  ```

#### Windows

* **On Windows, core pinning is only available locally (not in CI).**
* **To run locally without priority inheritance:**

  ```powershell
  # Use /affinity to run on a single core!
  start /wait /affinity 1 .\build\Debug\main.exe --no-inheritance
  # To terminate if it hangs, close the window or press Ctrl+C in the terminal.
  ```
* **With priority inheritance (default/fixed):**

  ```powershell
  start /wait /affinity 1 .\build\Debug\main.exe
  # To terminate, close the window or press Ctrl+C in the terminal.
  ```

#### macOS

* **Core pinning is not available.**
* **To run without priority inheritance:**

  ```bash
  ./build/main --no-inheritance
  # Press Ctrl+C to stop if it hangs.
  ```
* **With priority inheritance:**

  ```bash
  ./build/main
  # Press Ctrl+C to stop if needed.
  ```

*Note: On macOS, delays may be smaller and true deadlocks may not occur due to lack of core pinning.*

---

## Parameters

* `--no-inheritance` — Runs the simulation without priority inheritance (reproducing the bug).
* *(No arguments)* or `--inheritance` — Runs with priority inheritance enabled (bug fixed).

---
