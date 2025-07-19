# Mars Pathfinder Priority Inversion Simulation

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

---

## Why This Project?

* **Priority inversion** is not just theoretical; it has caused real mission failures and is a risk in any real-time, embedded, or concurrent system (spacecraft, aviation, robotics, automotive, etc.).
* **Priority inheritance** is a crucial OS-level mechanism to avoid such failures.
* This project **lets you experience and visualize** both the bug and its solution, on your PC or in CI, and learn fundamental lessons in concurrent programming and OS design.

---

## What Does This Project Demonstrate?

* **Reproduce the Mars Pathfinder bug:** See severe delays or virtual deadlock due to priority inversion.
* **See the effect of CPU affinity:** Forcing all threads onto a single core maximizes the inversion effect, just as it happened on the rover.
* **Observe the fix:** See how priority inheritance immediately solves the problem.
* **Compare behavior on Linux, Windows, and macOS:** Learn how operating system design affects real concurrency bugs.

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

* **Waited N ms:** How long the high-priority thread was blocked.
  In the real bug, this could be nearly indefinite. In the simulation, with affinity and CPU load, it can be very long.
* **\[Medium] Still hogging CPU...**: The medium-priority thread monopolizes the CPU, preventing progress.
* **\[Low] Priority temporarily raised to HIGH:** Priority inheritance is being simulated—the low-priority thread is boosted to release the mutex.

---

## Achieving the Mars Pathfinder Effect on Your Computer and in CI

* **Linux:** Use `taskset -c 0` to force all threads onto a single core.
* **Windows:** See below for CI limitations—on local machines, you can use `/affinity` to simulate single-core execution, but this is not available in CI.
* **In code:** The medium thread creates heavy CPU load (busy-wait loop) to preempt the low-priority thread.
* **In CI:** The workflow sets affinity and enforces a timeout to make the effect reproducible (**Linux only**).

With these settings, the high-priority thread can be delayed for many seconds or more, mimicking the real Mars Pathfinder bug. When priority inheritance is enabled, the problem is solved and delays drop sharply.

---

## Why This Cannot Be Fully Reproduced on macOS

* **macOS does not provide a standard tool for core pinning**—there is no equivalent to `taskset` or `/affinity`.
* The macOS scheduler is designed for “fairness”; every thread eventually gets CPU time, so real deadlocks do not occur as on a real-time OS.
* As a result, you will see delays, but not indefinite blocking as on VxWorks or with strict single-core affinity on Linux/Windows.
* Tools like `cpulimit` only restrict total CPU usage, not core affinity.

---

## Windows Support: No CI, No Core Pinning

> **Note:**
> There is **no Windows CI support for this project**.
> On Windows, the program runs on all available CPU cores (multi-core).
> Core pinning (forcing the program to run on a single core) is not available in GitHub Actions CI for Windows due to limitations of virtualization and cloud scheduling.
> Affinity settings in the cloud are unreliable—your process may not be assigned a real core, and boosted thread priorities may be ignored or overridden.
> As a result, the effect of priority inversion is less dramatic and less predictable on Windows, and may not match the classic Mars Pathfinder scenario.

---

## How to Compile and Run

### 1. Clone the Repository

```bash
git clone https://github.com/username/Mars-Pathfinder-Priority-Inversion.git
cd Mars-Pathfinder-Priority-Inversion
```

### 2. Build the Project

Requires CMake 3.16+ and a C++ compiler (gcc/g++ or clang/clang++).

```bash
cmake -S . -B build
cmake --build build
```

### 3. Run the Program

#### Linux

* **To simulate the Mars bug (no priority inheritance):**

  ```bash
  timeout 60s taskset -c 0 ./build/main --no-inheritance
  ```
* **With priority inheritance (default):**

  ```bash
  ./build/main
  ```

#### Windows

* **On Windows, only multi-core execution is available in CI.**
* **To run locally:** you may use

  ```powershell
  start /wait /affinity 1 .\build\Debug\main.exe --no-inheritance
  ```

  if your system supports it. In CI, core pinning is not supported.

#### macOS

* **With GNU timeout (no core pinning available):**

  ```bash
  gtimeout 60s ./build/main --no-inheritance
  ```
* **With priority inheritance:**

  ```bash
  ./build/main
  ```

*Note: On macOS, true CPU affinity cannot be set—delay may be smaller, and you will not observe a hard deadlock.*

---

## Parameters

* `--no-inheritance` — Runs the simulation without priority inheritance (reproducing the bug).
* *(No arguments)* or `--inheritance` — Runs with priority inheritance enabled (bug fixed).

---

## **Limitations of CI Environments and Windows**

> **Note:**
> In cloud CI environments such as GitHub Actions, Windows runners cannot reliably simulate single-core execution due to virtualization and cloud CPU scheduling. Affinity and thread priority settings are often ignored, and resource sharing with other virtual machines can make thread scheduling unpredictable.
>
> For the most reliable demonstration and analysis, prefer local testing on Linux with `taskset`, or on your own Windows machine if you can set affinity manually.
