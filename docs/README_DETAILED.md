# Multi-Core MESI Simulator

Cycle-accurate simulator for a 4-core pipelined processor with MESI cache coherence protocol.

## Architecture

- **4 cores**, each with:
  - Private instruction memory (I-MEM, 1024 words)
  - Private data cache (512 words, 8-word blocks, 64 lines)
  - 5-stage pipeline (Fetch → Decode → Execute → Memory → Writeback)
  - 16 registers (R0-R15)

- **Shared bus** with:
  - Round-robin arbitration
  - Commands: BusRd, BusRdX, Flush
  - MESI snooping protocol

- **Shared main memory** (2^21 words)

## Building

### Windows (Visual Studio)
```powershell
MSBuild sim.vcxproj /p:Configuration=Release /p:Platform=x64
```

The executable will be in `Release\sim.exe`

## Running

```powershell
cd tests\<testname>
..\..\Release\sim.exe
```

The simulator reads input files and generates 22 output files per test:
- `core0trace.txt` - `core3trace.txt`: Pipeline state per cycle
- `bustrace.txt`: Bus transactions
- `memout.txt`: Final memory state
- `regout0.txt` - `regout3.txt`: Final register values
- `dsram0.txt` - `dsram3.txt`: Cache data
- `tsram0.txt` - `tsram3.txt`: Cache tags and MESI states
- `stats0.txt` - `stats3.txt`: Performance statistics

## Testing

Run all tests:
```powershell
.\test_all.ps1
```

### Available Tests

1. **counter** - Basic test with data hazards and branch
2. **mulserial** - Matrix multiplication (serial execution on core 0)

## Features Implemented

### Pipeline
- ✓ 5-stage in-order pipeline
- ✓ Branch resolution in Decode stage
- ✓ Delay slot execution
- ✓ Data hazard detection (stall in Decode)
- ✓ No forwarding (register writes visible next cycle)
- ✓ R0 hardwired to 0
- ✓ R1 = sign-extended immediate

### Cache & Memory
- ✓ 512-word direct-mapped cache (8-word blocks, 64 lines)
- ✓ Write-back + write-allocate policy
- ✓ MESI coherence protocol (Modified, Exclusive, Shared, Invalid)
- ✓ Cache flush at end of simulation

### Bus & Arbitration
- ✓ Round-robin arbitration (fair)
- ✓ BusRd (read request)
- ✓ BusRdX (read-exclusive for writes)
- ✓ Flush (8-cycle data transfer)
- ✓ 16-cycle memory latency
- ✓ Snoop protocol for coherence

### Output & Tracing
- ✓ Cycle-accurate core traces
- ✓ Bus transaction trace
- ✓ Memory state output
- ✓ Register file dumps
- ✓ Cache dumps (DSRAM + TSRAM)
- ✓ Performance statistics (cycles, instructions, cache hits/misses)

## Project Structure

```
architecture-/
├── include/
│   └── sim.h              # Main header with all structures
├── src/
│   ├── main.c             # Entry point, I/O, tracing
│   ├── pipeline.c         # 5-stage pipeline implementation
│   ├── cache.c            # Cache operations & MESI protocol
│   └── bus.c              # Bus arbitration & memory controller
├── tests/
│   ├── counter/           # Test case 1
│   └── mulserial/         # Test case 2
├── sim.sln                # Visual Studio solution
├── sim.vcxproj            # Visual Studio project
├── generate_tests.py      # Test generator script
├── test_all.ps1           # Comprehensive test suite
└── README.md              # This file
```

## Requirements Met

- ✓ 4-core architecture
- ✓ Private I-MEM and data cache per core
- ✓ MESI cache coherence protocol
- ✓ Shared bus with round-robin arbitration
- ✓ Shared main memory (2^21 words)
- ✓ 5-stage pipeline with hazard detection
- ✓ Cycle-accurate simulation
- ✓ Complete trace and output generation
- ✓ Visual Studio project for Windows

## Performance

Example (counter test):
- Total cycles: 14
- Instructions: 8
- Cache hits: 0 (I-MEM only)

## Notes

- Simulation stops when all cores halt and pipelines drain
- Safety limit: 1M cycles (prevents infinite loops)
- Write-back cache requires flush at end to update main memory
- Branch delay slot always executes (even if branch not taken)
