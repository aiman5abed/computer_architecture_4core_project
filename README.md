# Multi-Core MESI Simulator

A cycle-accurate simulator for a 4-core processor with:
- Direct-mapped cache with MESI coherency protocol
- 5-stage pipelined cores (Fetch, Decode, Execute, Mem, Writeback)
- Shared bus with round-robin arbitration
- 21-bit address space (2 MW main memory)

## Project Structure

```
archtic/
├── src/               # Source code
│   ├── main.c         # Entry point, I/O, simulation loop
│   ├── pipeline.c     # 5-stage pipeline implementation
│   ├── cache.c        # Cache and MESI protocol
│   └── bus.c          # Shared bus and memory controller
├── include/           # Header files
│   └── sim.h          # Master header with all structs and constants
├── tests/             # Test programs
│   ├── counter/       # Counter test with expected outputs
│   └── simple/        # Basic test programs
├── build.bat          # Windows build script (MSVC)
├── Makefile           # Unix/MinGW build script
├── .gitignore         # Git ignore rules
└── README.md          # This file
```

## Specifications

### Memory System
- **Main Memory**: 2^21 words (21-bit address)
- **Cache**: 512 words per core (64 blocks × 8 words/block)
- **Block Size**: 8 words
- **Memory Response**: 16 cycles delay, then Flush transmits one word per cycle for 8 cycles (bus_addr increments across block words)

### Pipeline
- 5-stage: Fetch → Decode → Execute → Mem → Writeback
- Branch resolution in Decode stage
- Delay slot (instruction after branch always executes)
- No forwarding - data hazards cause decode stalls

### MESI Protocol
- **M (Modified)**: Exclusive, dirty - code 3
- **E (Exclusive)**: Exclusive, clean - code 2
- **S (Shared)**: Multiple copies may exist - code 1
- **I (Invalid)**: Not valid - code 0

## Building

### Visual Studio (Windows)
```
build.bat
```
Or from Developer Command Prompt:
```
cl /W3 /O2 /Fe:sim.exe main.c pipeline.c cache.c bus.c
```

### GCC (Linux/macOS/MinGW)
```
make
```

## Usage

```
sim.exe imem0.txt imem1.txt imem2.txt imem3.txt memin.txt memout.txt \
        regout0.txt regout1.txt regout2.txt regout3.txt \
        core0trace.txt core1trace.txt core2trace.txt core3trace.txt \
        bustrace.txt dsram0.txt dsram1.txt dsram2.txt dsram3.txt \
        tsram0.txt tsram1.txt tsram2.txt tsram3.txt \
        stats0.txt stats1.txt stats2.txt stats3.txt
```

### Command Line Arguments (27 total)
1-4: `imem0-3.txt` - Instruction memory for each core
5: `memin.txt` - Initial main memory contents
6: `memout.txt` - Final main memory
7-10: `regout0-3.txt` - Register outputs
11-14: `core0-3trace.txt` - Pipeline traces
15: `bustrace.txt` - Bus trace
16-19: `dsram0-3.txt` - Cache data
20-23: `tsram0-3.txt` - Cache tags
24-27: `stats0-3.txt` - Statistics

### Input Files
- `imem0-3.txt`: Instruction memory for each core (hex, one per line, max 1024)
- `memin.txt`: Initial main memory contents (hex, one per line)

### Output Files
- `memout.txt`: Final main memory contents
- `regout0-3.txt`: Final register values (R2-R15) for each core
- `core0-3trace.txt`: Pipeline trace for each core
- `bustrace.txt`: Bus transaction trace
- `dsram0-3.txt`: Data cache contents (512 lines each)
- `tsram0-3.txt`: Tag cache contents (64 lines each, packing: `(tag << 2) | mesi`)
- `stats0-3.txt`: Statistics for each core

## File Formats

### Core Trace Format
```
cycle fetch_PC decode_PC exec_PC mem_PC wb_PC R2 R3 R4 ... R15
```
- Cycle: decimal
- PCs: 3 hex digits (or `---` if stage empty)
- Registers R2-R15: 8 hex digits each
- **Registers are printed at the beginning of the cycle (pre-state), before any writeback occurs**

### Bus Trace Format
```
cycle origid cmd addr data shared
```
- cycle: decimal
- origid: **1 hex digit** (0-3 for cores, 4 for main memory)
- cmd: **1 hex digit** (0=None, 1=BusRd, 2=BusRdX, 3=Flush)
- addr: **6 hex digits** (21-bit address)
- data: **8 hex digits**
- shared: **1 hex digit** (0 or 1)

### Stats Format
```
cycles <decimal>
instructions <decimal>
read_hit <decimal>
write_hit <decimal>
read_miss <decimal>
write_miss <decimal>
decode_stall <decimal>
mem_stall <decimal>
```

## Instruction Set (32-bit format)

```
| opcode (8) | rd (4) | rs (4) | rt (4) | immediate (12) |
   31-24       23-20    19-16    15-12       11-0
```

| Opcode | Name | Description |
|--------|------|-------------|
| 0 | ADD | R[rd] = R[rs] + R[rt] |
| 1 | SUB | R[rd] = R[rs] - R[rt] |
| 2 | AND | R[rd] = R[rs] & R[rt] |
| 3 | OR | R[rd] = R[rs] \| R[rt] |
| 4 | XOR | R[rd] = R[rs] ^ R[rt] |
| 5 | MUL | R[rd] = R[rs] × R[rt] |
| 6 | SLL | R[rd] = R[rs] << R[rt] |
| 7 | SRA | R[rd] = R[rs] >> R[rt] (arithmetic) |
| 8 | SRL | R[rd] = R[rs] >> R[rt] (logical) |
| 9 | BEQ | if (R[rs] == R[rt]) PC = R[rd][9:0] |
| 10 | BNE | if (R[rs] != R[rt]) PC = R[rd][9:0] |
| 11 | BLT | if (R[rs] < R[rt]) PC = R[rd][9:0] (signed) |
| 12 | BGT | if (R[rs] > R[rt]) PC = R[rd][9:0] (signed) |
| 13 | BLE | if (R[rs] <= R[rt]) PC = R[rd][9:0] (signed) |
| 14 | BGE | if (R[rs] >= R[rt]) PC = R[rd][9:0] (signed) |
| 15 | JAL | R[15] = PC+1, PC = R[rd][9:0] |
| 16 | LW | R[rd] = MEM[R[rs] + R[rt]] |
| 17 | SW | MEM[R[rs] + R[rt]] = R[rd] |
| 21 | HALT | Stop execution |

**Special Registers**:
- R0 is hardwired to 0 (writes ignored)
- R1 contains sign-extended 12-bit immediate of current instruction

## Address Breakdown

For cache access (21-bit address):
```
| Tag (12 bits) | Index (6 bits) | Block Offset (3 bits) |
    20-9            8-3                2-0
```

## Test Programs

### counter/
Simple test with ALU operations and halt.

```
# imem0.txt - Core 0 does some work
00200001    # ADD R2, R0, R1 (R2 = 1)
01102001    # SUB R2, R2, R1 (R2 = 0)  
00221001    # ADD R2, R2, R1 (R2 = 1)
0E104003    # BGE R4, R0, 0x003 (loop if R4 >= 0)
15000000    # HALT

# imem1-3.txt - Other cores just halt
15000000    # HALT
```

## License

Educational use.
