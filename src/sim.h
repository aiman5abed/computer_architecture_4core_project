/*
 * =============================================================================
 * Multi-Core Processor Simulator - Master Header File
 * =============================================================================
 * 4-Core MESI Cache Coherent Processor Simulator
 * Cycle-accurate simulation per PDF specification
 * =============================================================================
 */

#ifndef SIM_H
#define SIM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* =============================================================================
 * SYSTEM CONSTANTS (FROM PDF SPEC)
 * =============================================================================
 */

// Core Configuration
#define NUM_CORES           4
#define NUM_REGISTERS       16
#define REGISTER_WIDTH      32      // bits

// Memory Configuration
#define IMEM_DEPTH          1024    // Instructions per core
#define MAIN_MEM_SIZE       (1 << 21)  // 2^21 words = 2097152 words
#define WORD_SIZE           32      // bits

// PC Configuration
#define PC_WIDTH            10      // bits
#define PC_MASK             0x3FF   // 10-bit mask

// Cache Configuration (Direct-Mapped) - FROM PDF
#define CACHE_SIZE          512     // Total DSRAM words (512)
#define CACHE_BLOCK_SIZE    8       // 8 words per block
#define CACHE_NUM_BLOCKS    64      // 512 / 8 = 64 blocks (TSRAM lines)
#define BLOCK_OFFSET_BITS   3       // log2(8) = 3 bits for word offset
#define INDEX_BITS          6       // log2(64) = 6 bits for cache index
#define TAG_BITS            12      // 21 - 3 - 6 = 12 bits for tag

// Bus Configuration
#define BUS_ADDR_BITS       21      // 21-bit word address

// Timing Constants
#define MEM_RESPONSE_DELAY  16      // cycles before first Flush word from memory

/* =============================================================================
 * INSTRUCTION SET ARCHITECTURE
 * =============================================================================
 * 
 * Instruction Format (32 bits):
 * +--------+----+----+----+--------------------+
 * | opcode | rd | rs | rt |     immediate      |
 * +--------+----+----+----+--------------------+
 *   31-24   23-20 19-16 15-12      11-0
 *   8 bits  4 bits 4bits 4bits    12 bits
 */

// Opcodes
typedef enum {
    OP_ADD  = 0,    // rd = rs + rt
    OP_SUB  = 1,    // rd = rs - rt
    OP_AND  = 2,    // rd = rs & rt
    OP_OR   = 3,    // rd = rs | rt
    OP_XOR  = 4,    // rd = rs ^ rt
    OP_MUL  = 5,    // rd = rs * rt
    OP_SLL  = 6,    // rd = rs << rt
    OP_SRA  = 7,    // rd = rs >> rt (arithmetic)
    OP_SRL  = 8,    // rd = rs >> rt (logical)
    OP_BEQ  = 9,    // if (rs == rt) pc = rd[9:0]
    OP_BNE  = 10,   // if (rs != rt) pc = rd[9:0]
    OP_BLT  = 11,   // if (rs < rt) pc = rd[9:0] (signed)
    OP_BGT  = 12,   // if (rs > rt) pc = rd[9:0] (signed)
    OP_BLE  = 13,   // if (rs <= rt) pc = rd[9:0] (signed)
    OP_BGE  = 14,   // if (rs >= rt) pc = rd[9:0] (signed)
    OP_JAL  = 15,   // R[15] = pc+1, pc = rd[9:0]
    OP_LW   = 16,   // rd = MEM[rs + rt]
    OP_SW   = 17,   // MEM[rs + rt] = rd
    OP_RSVD = 18,   // Reserved
    OP_IN   = 19,   // Reserved (not used in this sim)
    OP_OUT  = 20,   // Reserved (not used in this sim)
    OP_HALT = 21    // Stop core (opcode 21 = 0x15)
} Opcode;

// Decoded instruction
typedef struct {
    uint32_t raw;
    uint8_t  opcode;
    uint8_t  rd;
    uint8_t  rs;
    uint8_t  rt;
    int32_t  immediate;  // Sign-extended 12-bit
} Instruction;

/* =============================================================================
 * MESI CACHE COHERENCY
 * =============================================================================
 */

// MESI States: 0=I, 1=S, 2=E, 3=M
typedef enum {
    MESI_INVALID   = 0,
    MESI_SHARED    = 1,
    MESI_EXCLUSIVE = 2,
    MESI_MODIFIED  = 3
} MESIState;

// Bus Commands: 0=none, 1=BusRd, 2=BusRdX, 3=Flush
typedef enum {
    BUS_CMD_NONE  = 0,
    BUS_CMD_BUSRD = 1,
    BUS_CMD_BUSRDX = 2,
    BUS_CMD_FLUSH = 3
} BusCommand;

// Bus originator IDs
#define BUS_ORIG_MEMORY     4

// TSRAM Entry: {MESI(2 bits), Tag(12 bits)}
typedef struct {
    uint32_t  tag;   // 12 bits
    MESIState mesi;  // 2 bits
} TSRAMEntry;

// Cache
typedef struct {
    int32_t    dsram[CACHE_SIZE];           // 512 words (signed for proper handling)
    TSRAMEntry tsram[CACHE_NUM_BLOCKS];     // 64 entries
} Cache;

/* =============================================================================
 * PIPELINE STRUCTURES
 * =============================================================================
 */

typedef struct {
    bool        valid;
    uint32_t    pc;             // PC of this instruction
    Instruction inst;
    int32_t     rs_val;         // Value of rs register
    int32_t     rt_val;         // Value of rt register
    int32_t     rd_val;         // Value of rd register (for SW and branches)
    int32_t     alu_result;     // ALU output
    int32_t     mem_data;       // Data from memory (LW)
} PipelineLatch;

typedef struct {
    int             core_id;
    
    // PC
    uint32_t        pc;
    
    // Registers (current state - reads return this value)
    int32_t         regs[NUM_REGISTERS];
    
    // IMEM (private)
    uint32_t        imem[IMEM_DEPTH];
    
    // Pipeline latches (current state)
    PipelineLatch   IF_ID;      // Fetch -> Decode
    PipelineLatch   ID_EX;      // Decode -> Execute
    PipelineLatch   EX_MEM;     // Execute -> Mem
    PipelineLatch   MEM_WB;     // Mem -> Writeback
    PipelineLatch   WB_completed; // Tracks what completed WB last cycle (for trace)
    
    // Cache
    Cache           cache;
    
    // Control
    bool            halted;             // HALT executed (pipeline may still drain)
    bool            decode_stall;       // Data hazard stall
    bool            mem_stall;          // Cache miss stall
    bool            waiting_for_bus;    // Bus transaction pending
    bool            fetch_enabled;      // Can fetch new instructions
    
    // Pending bus request
    bool            bus_request_pending;
    BusCommand      pending_bus_cmd;
    uint32_t        pending_bus_addr;   // Block-aligned
    bool            pending_is_write;   // True if write miss
    int32_t         pending_write_data; // Data to write after fill
    uint32_t        pending_store_addr; // Full address for store
    
    // Statistics
    uint32_t        cycle_count;
    uint32_t        instruction_count;
    uint32_t        read_hits;
    uint32_t        write_hits;
    uint32_t        read_misses;
    uint32_t        write_misses;
    uint32_t        decode_stall_cycles;
    uint32_t        mem_stall_cycles;
} Core;

/* =============================================================================
 * BUS AND MEMORY
 * =============================================================================
 */

typedef struct {
    bool        active;
    BusCommand  cmd;
    int         origid;         // 0-3 = core, 4 = memory
    uint32_t    addr;           // 21-bit word address
    int32_t     data;           // 32-bit data (signed for memory values)
    bool        shared;         // bus_shared signal
} BusState;

typedef struct {
    bool        valid;
    int         requesting_core;
    uint32_t    block_addr;
    int         cycles_remaining;   // Countdown to first word
    int         words_sent;         // 0-7
    bool        is_rdx;
    int         data_source;        // -1=memory, 0-3=cache with M
    bool        shared;             // Was bus_shared set during request?
} MemoryResponse;

typedef struct {
    int         last_granted;   // For round-robin (lowest priority next)
    bool        transaction_in_progress;  // BusRd/BusRdX not yet completed
} BusArbiter;

typedef struct {
    BusState        state;          // Current bus state
    BusArbiter      arbiter;
    MemoryResponse  mem_response;
    
    // Snoop results (set during arbitration)
    bool            snoop_shared;           // Any cache has the block
    bool            snoop_has_modified;     // Some cache has M state
    int             snoop_modified_core;    // Which core has M (-1 if none)
} Bus;

/* =============================================================================
 * SIMULATOR STATE
 * =============================================================================
 */

typedef struct {
    Core        cores[NUM_CORES];
    int32_t*    main_memory;    // 2^21 words, dynamically allocated
    Bus         bus;
    uint64_t    cycle;
    
    // Trace files
    FILE*       core_trace[NUM_CORES];
    FILE*       bus_trace;
} Simulator;

/* =============================================================================
 * FUNCTION PROTOTYPES
 * =============================================================================
 */

// Init
void sim_init(Simulator* sim);
void core_init(Core* core, int id);
void cache_init(Cache* cache);
void bus_init(Bus* bus);
void sim_cleanup(Simulator* sim);

// File I/O
bool load_imem(Core* core, const char* filename);
bool load_memin(Simulator* sim, const char* filename);
void write_memout(Simulator* sim, const char* filename);
void write_regout(Core* core, const char* filename);
void write_dsram(Core* core, const char* filename);
void write_tsram(Core* core, const char* filename);
void write_stats(Core* core, const char* filename);

// Instructions
Instruction decode_instruction(uint32_t raw);
int32_t sign_extend_12(uint32_t value);

// Pipeline
void core_cycle(Core* core, Simulator* sim);
bool check_data_hazard(Core* core);
int  get_dest_reg(PipelineLatch* latch);

// Cache
bool cache_read(Core* core, Simulator* sim, uint32_t addr, int32_t* data);
bool cache_write(Core* core, Simulator* sim, uint32_t addr, int32_t data);
int  cache_get_index(uint32_t addr);
uint32_t cache_get_tag(uint32_t addr);
uint32_t cache_get_block_addr(uint32_t addr);
int  cache_get_offset(uint32_t addr);
void cache_writeback_block(Core* core, Simulator* sim, int index);

// Bus
void bus_cycle(Simulator* sim);
int  bus_arbitrate(Simulator* sim);
void bus_snoop(Simulator* sim, BusCommand cmd, uint32_t addr, int requester);
void bus_issue_request(Core* core, BusCommand cmd, uint32_t block_addr);

// Memory
void memory_cycle(Simulator* sim);

// Trace
void trace_core(Simulator* sim, int core_id);
void trace_bus(Simulator* sim);

// Control
bool all_cores_done(Simulator* sim);
bool pipeline_active(Core* core);
void run_simulation(Simulator* sim);

// MESI snooping
void mesi_snoop_busrd(Core* core, Simulator* sim, uint32_t block_addr, int requester);
void mesi_snoop_busrdx(Core* core, Simulator* sim, uint32_t block_addr, int requester);

#endif // SIM_H
