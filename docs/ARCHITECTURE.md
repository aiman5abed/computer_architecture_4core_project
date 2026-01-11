# Multi-Core MESI Simulator - Architecture Diagram

## System Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        MULTI-CORE MESI SIMULATOR                            │
│                           (4-Core Architecture)                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│   CORE 0     │  │   CORE 1     │  │   CORE 2     │  │   CORE 3     │
├──────────────┤  ├──────────────┤  ├──────────────┤  ├──────────────┤
│              │  │              │  │              │  │              │
│ ┌──────────┐ │  │ ┌──────────┐ │  │ ┌──────────┐ │  │ ┌──────────┐ │
│ │  I-MEM   │ │  │ │  I-MEM   │ │  │ │  I-MEM   │ │  │ │  I-MEM   │ │
│ │ (1024 W) │ │  │ │ (1024 W) │ │  │ │ (1024 W) │ │  │ │ (1024 W) │ │
│ └─────┬────┘ │  │ └─────┬────┘ │  │ └─────┬────┘ │  │ └─────┬────┘ │
│       │      │  │       │      │  │       │      │  │       │      │
│ ┌─────▼────┐ │  │ ┌─────▼────┐ │  │ ┌─────▼────┐ │  │ ┌─────▼────┐ │
│ │ Pipeline │ │  │ │ Pipeline │ │  │ │ Pipeline │ │  │ │ Pipeline │ │
│ │ (5-stage)│ │  │ │ (5-stage)│ │  │ │ (5-stage)│ │  │ │ (5-stage)│ │
│ │  IF→ID→  │ │  │ │  IF→ID→  │ │  │ │  IF→ID→  │ │  │ │  IF→ID→  │ │
│ │  EX→MEM→ │ │  │ │  EX→MEM→ │ │  │ │  EX→MEM→ │ │  │ │  EX→MEM→ │ │
│ │    WB    │ │  │ │    WB    │ │  │ │    WB    │ │  │ │    WB    │ │
│ └─────┬────┘ │  │ └─────┬────┘ │  │ └─────┬────┘ │  │ └─────┬────┘ │
│       │      │  │       │      │  │       │      │  │       │      │
│ ┌─────▼────┐ │  │ ┌─────▼────┐ │  │ ┌─────▼────┐ │  │ ┌─────▼────┐ │
│ │ D-Cache  │ │  │ │ D-Cache  │ │  │ │ D-Cache  │ │  │ │ D-Cache  │ │
│ │ (512 W)  │ │  │ │ (512 W)  │ │  │ │ (512 W)  │ │  │ │ (512 W)  │ │
│ │ DSRAM    │ │  │ │ DSRAM    │ │  │ │ DSRAM    │ │  │ │ DSRAM    │ │
│ │ TSRAM    │ │  │ │ TSRAM    │ │  │ │ TSRAM    │ │  │ │ TSRAM    │ │
│ │ MESI: M/E│ │  │ │ MESI: M/E│ │  │ │ MESI: M/E│ │  │ │ MESI: M/E│ │
│ │      S/I │ │  │ │      S/I │ │  │ │      S/I │ │  │ │      S/I │ │
│ └─────┬────┘ │  │ └─────┬────┘ │  │ └─────┬────┘ │  │ └─────┬────┘ │
│       │      │  │       │      │  │       │      │  │       │      │
│   BusRd│RdX  │  │   BusRd│RdX  │  │   BusRd│RdX  │  │   BusRd│RdX  │
│   Snoop│      │  │   Snoop│      │  │   Snoop│      │  │   Snoop│      │
└───────┼──────┘  └───────┼──────┘  └───────┼──────┘  └───────┼──────┘
        │                 │                 │                 │
        └─────────────────┴─────────────────┴─────────────────┘
                                  │
        ┌─────────────────────────▼─────────────────────────────┐
        │              SHARED BUS (with Arbitration)            │
        │  ┌────────────────────────────────────────────────┐   │
        │  │  Commands: BusRd | BusRdX | Flush              │   │
        │  │  Signals: origid, addr, data, shared           │   │
        │  │  Arbitration: Round-Robin (fair)               │   │
        │  │  Snoop: Broadcast to all caches                │   │
        │  └────────────────────────────────────────────────┘   │
        └───────────────────────────┬───────────────────────────┘
                                    │
        ┌───────────────────────────▼───────────────────────────┐
        │            MAIN MEMORY (2^21 words)                   │
        │  ┌────────────────────────────────────────────────┐   │
        │  │  Response: 16 cycles + 8 Flush cycles          │   │
        │  │  Block size: 8 words                           │   │
        │  │  Stores final program results                  │   │
        │  └────────────────────────────────────────────────┘   │
        └───────────────────────────────────────────────────────┘
```

## Core Pipeline Detail

```
Each Core has a 5-Stage Pipeline:

┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐
│  FETCH  │──▶│ DECODE  │──▶│ EXECUTE │──▶│ MEMORY  │──▶│WRITEBACK│
│  (IF)   │   │  (ID)   │   │  (EX)   │   │  (MEM)  │   │  (WB)   │
└────┬────┘   └────┬────┘   └────┬────┘   └────┬────┘   └────┬────┘
     │             │             │             │             │
  Read I-MEM   Branch Res.    ALU Ops      Cache R/W     Reg Write
             Hazard Check                  Bus Req       Commit Inst
             Stall on RAW                  Stall on Miss
```

### Pipeline Features:
- **IF**: Fetch instruction from private I-MEM
- **ID**: Decode, resolve branches, check hazards, read registers
- **EX**: Execute ALU operations
- **MEM**: Access data cache (may stall on miss)
- **WB**: Write back to register file (R0 hardwired to 0, R1=immediate)

### Hazard Handling:
- **Data Hazards**: Stall in DECODE if register in flight (ID_EX, EX_MEM, MEM_WB)
- **Branch**: Resolved in DECODE with delay slot execution
- **Cache Miss**: Stall in MEMORY until data arrives via bus

## Cache Architecture

```
Direct-Mapped Cache (per core):
┌────────────────────────────────────────┐
│  Total: 512 words                      │
│  Block Size: 8 words                   │
│  Lines: 64 (512/8)                     │
│                                        │
│  ┌──────────────┐  ┌──────────────┐   │
│  │    DSRAM     │  │    TSRAM     │   │
│  │  Data Store  │  │  Tag + MESI  │   │
│  │  512 words   │  │   64 lines   │   │
│  └──────────────┘  └──────────────┘   │
│                                        │
│  Address Breakdown (21-bit):          │
│  ┌────────┬───────┬────────┐          │
│  │  Tag   │ Index │ Offset │          │
│  │ 12 bit │ 6 bit │ 3 bit  │          │
│  └────────┴───────┴────────┘          │
│                                        │
│  Policy: Write-Back + Write-Allocate  │
└────────────────────────────────────────┘
```

## MESI Protocol State Machine

```
┌──────────────────────────────────────────────────────────────┐
│                    MESI COHERENCE PROTOCOL                   │
└──────────────────────────────────────────────────────────────┘

        ┌──────────────────────────────────────────┐
        │             INVALID (I)                  │
        │        (Cache line not present)          │
        └────┬───────────────────────────────┬─────┘
             │                               │
        BusRd│ (no sharing)            BusRdX│
             │                               │
        ┌────▼────────┐                 ┌────▼────────┐
        │ EXCLUSIVE   │   CPU Write     │  MODIFIED   │
        │     (E)     │────────────────▶│     (M)     │
        │  Clean Copy │                 │ Dirty Copy  │
        └────┬────────┘                 └────┬────────┘
             │                               │
        BusRd│ (sharing)              BusRd/│BusRdX
             │   detected                   │(must flush)
        ┌────▼─────────────────────────────▶▼────────┐
        │             SHARED (S)                     │
        │     (Clean, other caches may have copy)    │
        └────────────────────┬───────────────────────┘
                             │
                      BusRdX│ (upgrade to exclusive)
                             │
                        ┌────▼────────┐
                        │  MODIFIED   │
                        │     (M)     │
                        └─────────────┘

State Transitions:
• I → E:  Read miss, no other cache has it (BusRd, shared=0)
• I → S:  Read miss, other cache has it (BusRd, shared=1)
• I → M:  Write miss (BusRdX)
• E → M:  CPU writes to clean exclusive line
• E → S:  Other cache reads (snoop BusRd)
• S → M:  CPU writes (BusRdX to invalidate others)
• S → I:  Other cache writes (snoop BusRdX)
• M → S:  Other cache reads (snoop BusRd, supply data)
• M → I:  Other cache writes (snoop BusRdX, supply data)
```

## Bus Transaction Flow

```
┌────────────────────────────────────────────────────────────┐
│               BUS TRANSACTION TIMELINE                     │
└────────────────────────────────────────────────────────────┘

Cycle 0:  Core requests bus (cache miss)
          ↓
Cycle 1:  Arbitration (round-robin)
          Core wins bus
          ↓
Cycle 2:  BusRd/BusRdX issued on bus
          All caches snoop
          - Check tags
          - Set shared signal
          - Identify if M state exists
          ↓
Cycle 3:  Memory response starts
          16-cycle delay countdown
          ↓
Cycle 18: First Flush cycle
          Word 0 transferred
          ↓
Cycle 19: Flush word 1
          ↓
Cycle 20: Flush word 2
          ↓
  ...     (8 total flush cycles)
          ↓
Cycle 25: Flush word 7 (last)
          Transaction complete
          Core receives data
          MESI state updated
          Core resumes execution
```

## Data Flow Example

```
Example: Core 0 reads address 0x100, Core 1 has it in M state

┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
│ Core 0  │    │ Core 1  │    │   Bus   │    │  Memory │
└────┬────┘    └────┬────┘    └────┬────┘    └────┬────┘
     │              │              │              │
     │ Miss: 0x100  │              │              │
     ├─────────────▶│              │              │
     │              │              │              │
     │         BusRd│0x100         │              │
     ├──────────────┼─────────────▶│              │
     │              │              │              │
     │         Snoop│0x100         │              │
     │         ◀────┤              │              │
     │         M→S  │              │              │
     │         shared=1             │              │
     │              ├─────────────▶│              │
     │              │              │              │
     │              │ Supply data  │              │
     │              │  (8 Flush)   │              │
     │         ◀────┼──────────────┤              │
     │  MESI: S     │              │              │
     │              │  MESI: S     │              │
     │              │              │ Write-back   │
     │              │              ├─────────────▶│
     │              │              │              │
```

## File Structure & Data Flow

```
┌──────────────────────────────────────────────────────────────┐
│                    SIMULATOR COMPONENTS                      │
└──────────────────────────────────────────────────────────────┘

┌─────────────┐         ┌─────────────┐         ┌─────────────┐
│   main.c    │         │ pipeline.c  │         │  cache.c    │
├─────────────┤         ├─────────────┤         ├─────────────┤
│• File I/O   │────────▶│• IF stage   │────────▶│• cache_read │
│• Tracing    │         │• ID stage   │         │• cache_write│
│• sim_init   │         │• EX stage   │         │• MESI snoop │
│• sim_cycle  │         │• MEM stage  │         │• Writeback  │
│  loop       │         │• WB stage   │         └──────┬──────┘
└──────┬──────┘         │• Hazard det │                │
       │                └──────┬──────┘                │
       │                       │                       │
       ▼                       ▼                       ▼
┌─────────────┐         ┌─────────────┐         ┌─────────────┐
│   bus.c     │         │   sim.h     │         │ Output Files│
├─────────────┤         ├─────────────┤         ├─────────────┤
│• Arbitrate  │         │• Structures │         │• traces     │
│• Snoop      │         │• Core       │         │• memout     │
│• Memory     │         │• Cache      │         │• regout     │
│  response   │         │• Bus        │         │• dsram/tsram│
│• Flush data │         │• Simulator  │         │• stats      │
└─────────────┘         └─────────────┘         └─────────────┘
```

## Output Files Generated

```
For each test run, 22 files are generated:

PER-CORE FILES (×4 cores = 20 files):
├── core0trace.txt ... core3trace.txt  ← Pipeline state every cycle
├── regout0.txt ... regout3.txt        ← Final register values
├── dsram0.txt ... dsram3.txt          ← Cache data dump
├── tsram0.txt ... tsram3.txt          ← Cache tags + MESI states
└── stats0.txt ... stats3.txt          ← Performance counters

SHARED FILES (2 files):
├── bustrace.txt                       ← Bus transactions log
└── memout.txt                         ← Final main memory state

TRACE FORMAT:
Cycle | IF | ID | EX | MEM | WB | R0 | R1 | ... | R15
  1   |000 |--- |--- |---  |--- | 00 | 00 | ... | 00
  2   |001 |000 |--- |---  |--- | 00 | 01 | ... | 00
```

## Performance Metrics

```
Statistics tracked per core:

┌───────────────────────────────────────┐
│  stats0.txt (example)                 │
├───────────────────────────────────────┤
│  cycles 67              ← Total cycles│
│  instructions 5    ← Committed insts  │
│  read_hit 2        ← Cache read hits  │
│  read_miss 2       ← Cache read miss  │
│  write_hit 1       ← Cache write hits │
│  write_miss 1      ← Cache write miss │
│  decode_stall_cycles 0 ← Hazard stalls│
│  mem_stall_cycles 42   ← Cache stalls │
└───────────────────────────────────────┘

CPI = cycles / instructions = 67 / 5 = 13.4
(High CPI due to cache misses and memory latency)
```

---

## Summary

**This simulator implements:**
- 4-core shared-memory multiprocessor
- MESI cache coherence protocol
- 5-stage in-order pipeline per core
- Round-robin bus arbitration
- Write-back cache with proper flushing
- Cycle-accurate simulation with complete tracing
- All output files matching specification

**Total Lines of Code:** ~2,500 lines C
**Tested Configurations:** Counter test (passes), Mulserial test (verified)
