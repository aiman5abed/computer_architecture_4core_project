# Multi-Core MESI Simulator

A cycle-accurate 4-core MIPS processor simulator that implements the **MESI cache coherence protocol**. This project simulates a multi-core architecture with L1 caches, a shared bus, and main memory, capable of running parallel assembly programs.

## 🚀 Quick Start

### Prerequisites
* **OS:** Windows 10/11
* **Compiler:** Visual Studio Build Tools (C++ Desktop Development workload)
* **Shell:** `x64 Native Tools Command Prompt` (Find this in your Start Menu)

### How to Build & Run
1.  **Compile the Simulator:**
    ```cmd
    cl /Fe:sim.exe src/main.c src/pipeline.c src/cache.c src/bus.c
    ```

2.  **Generate a Test Case (e.g., Matrix Multiplication):**
    ```cmd
    cl testgen.c
    testgen.exe
    ```
    *(This creates `memin.txt` and `imem0-3.txt` files automatically)*

3.  **Run the Simulation:**
    ```cmd
    sim.exe
    ```

4.  **Check Results:**
    Open `memout.txt` to see the final memory state, or `stats0.txt` for Core 0 performance metrics.

---
*For a complete guide on the build process, test generation, and troubleshooting, see [docs/BUILDING.md](docs/BUILDING.md).*
