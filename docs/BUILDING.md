# Building and Testing Guide

This document details the build system, test generation workflow, and troubleshooting steps for the Multi-Core MESI Simulator.

## 🛠️ Build System Architecture

We utilize the Microsoft Visual C++ Compiler (`cl.exe`) directly from the command line. This method was chosen over `MSBuild` or Visual Studio Solutions (`.sln`) to avoid version mismatch errors (e.g., missing specific Windows SDK versions) and to provide a lightweight, portable build process.

### Prerequisites
To build this project, you must have **Visual Studio Build Tools** installed with the following components:
* **Desktop development with C++** (Workload)
* **Windows 10 SDK** (Individual Component - critical for `stdio.h`)

### Directory Structure
* `src/`: Contains the C source code for the simulator components (`main.c`, `bus.c`, `cache.c`, `pipeline.c`).
* `tests/`: Storage for archived test cases (Counter, Serial Matrix, Parallel Matrix).
* `ide/`: Legacy Visual Studio project files (deprecated).
* `testgen.c`: A utility program to generate machine code (Hex) and memory inputs.

---

## ⚙️ The Test Generation Workflow

Writing raw Hex machine code for a 4-core processor is error-prone. We use a **Generator Approach**:

1.  **Logic Definition:** We write the test logic (e.g., Matrix Multiplication loops) in C inside `testgen.c` using helper macros for MIPS opcodes (`OP_ADD`, `OP_BNE`, etc.).
2.  **Compilation:** We compile the generator:
    ```cmd
    cl testgen.c
    ```
3.  **Execution:** We run `testgen.exe`. This program:
    * Generates `memin.txt` (Main Memory initialized with data).
    * Generates `imem0.txt`, `imem1.txt`, `imem2.txt`, `imem3.txt` (Instruction Memory for each core).
4.  **Simulation:** The simulator (`sim.exe`) reads these generated text files at runtime.

**Why this matters:**
* It ensures instructions are correctly encoded (32-bit Hex).
* It calculates branch offsets (jumps) automatically.
* It prevents file encoding issues (e.g., PowerShell saving as UTF-16) by writing standard ASCII files.

---

## 🔧 Step-by-Step Build Process

### 1. Environment Setup
Always use the **x64 Native Tools Command Prompt for VS 2022**. Standard `cmd.exe` or PowerShell will likely fail with "command not found" errors because they lack the necessary environment variables (`INCLUDE`, `LIB`).

### 2. Compiling the Simulator
Run the following command from the project root:
```cmd
cl /Fe:sim.exe src/main.c src/pipeline.c src/cache.c src/bus.c
