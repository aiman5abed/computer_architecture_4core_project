/*
 * =============================================================================
 * Multi-Core Processor Simulator - Main Entry Point
 * =============================================================================
 * Handles command line parsing, file I/O, and simulation loop
 * =============================================================================
 */

#include "sim.h"

// Global simulator
static Simulator g_sim;

/* =============================================================================
 * INITIALIZATION
 * =============================================================================
 */

void sim_init(Simulator* sim) {
    memset(sim, 0, sizeof(Simulator));
    
    // Allocate main memory (2^21 words)
    sim->main_memory = (int32_t*)calloc(MAIN_MEM_SIZE, sizeof(int32_t));
    if (!sim->main_memory) {
        fprintf(stderr, "Error: Failed to allocate main memory\n");
        exit(1);
    }
    
    // Init cores
    for (int i = 0; i < NUM_CORES; i++) {
        core_init(&sim->cores[i], i);
    }
    
    // Init bus
    bus_init(&sim->bus);
    
    sim->cycle = 0;
}

void core_init(Core* core, int id) {
    memset(core, 0, sizeof(Core));
    core->core_id = id;
    core->pc = 0;
    
    // All registers start at 0
    for (int i = 0; i < NUM_REGISTERS; i++) {
        core->regs[i] = 0;
    }
    
    // IMEM zeroed
    memset(core->imem, 0, sizeof(core->imem));
    
    // Pipeline latches invalid
    core->IF_ID.valid = false;
    core->ID_EX.valid = false;
    core->EX_MEM.valid = false;
    core->MEM_WB.valid = false;
    core->WB_completed.valid = false;
    
    // Cache init
    cache_init(&core->cache);
    
    // Control flags
    core->halted = false;
    core->decode_stall = false;
    core->mem_stall = false;
    core->waiting_for_bus = false;
    core->fetch_enabled = true;
    core->bus_request_pending = false;
    
    // Stats
    core->cycle_count = 0;
    core->instruction_count = 0;
    core->read_hits = 0;
    core->write_hits = 0;
    core->read_misses = 0;
    core->write_misses = 0;
    core->decode_stall_cycles = 0;
    core->mem_stall_cycles = 0;
}

void cache_init(Cache* cache) {
    // DSRAM and TSRAM both zero at init
    memset(cache->dsram, 0, sizeof(cache->dsram));
    for (int i = 0; i < CACHE_NUM_BLOCKS; i++) {
        cache->tsram[i].tag = 0;
        cache->tsram[i].mesi = MESI_INVALID;
    }
}

void bus_init(Bus* bus) {
    memset(bus, 0, sizeof(Bus));
    bus->arbiter.last_granted = NUM_CORES - 1;  // Core 0 has highest priority first
    bus->arbiter.transaction_in_progress = false;
    bus->mem_response.valid = false;
    bus->snoop_modified_core = -1;
}

void sim_cleanup(Simulator* sim) {
    if (sim->main_memory) {
        free(sim->main_memory);
        sim->main_memory = NULL;
    }
    for (int i = 0; i < NUM_CORES; i++) {
        if (sim->core_trace[i]) fclose(sim->core_trace[i]);
    }
    if (sim->bus_trace) fclose(sim->bus_trace);
}

/* =============================================================================
 * INSTRUCTION DECODING
 * =============================================================================
 */

int32_t sign_extend_12(uint32_t value) {
    if (value & 0x800) {  // Bit 11 set = negative
        return (int32_t)(value | 0xFFFFF000);
    }
    return (int32_t)(value & 0xFFF);
}

Instruction decode_instruction(uint32_t raw) {
    Instruction inst;
    inst.raw = raw;
    inst.opcode = (raw >> 24) & 0xFF;
    inst.rd = (raw >> 20) & 0x0F;
    inst.rs = (raw >> 16) & 0x0F;
    inst.rt = (raw >> 12) & 0x0F;
    inst.immediate = sign_extend_12(raw & 0xFFF);
    return inst;
}

/* =============================================================================
 * FILE I/O
 * =============================================================================
 */

bool load_imem(Core* core, const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Warning: Cannot open IMEM file %s\n", filename);
        return false;
    }
    
    char line[64];
    int addr = 0;
    while (fgets(line, sizeof(line), fp) && addr < IMEM_DEPTH) {
        // Skip empty lines and whitespace
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\r' || *p == '\0') continue;
        
        uint32_t inst;
        if (sscanf(p, "%x", &inst) == 1) {
            core->imem[addr++] = inst;
        }
    }
    fclose(fp);
    return true;
}

bool load_memin(Simulator* sim, const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Warning: Cannot open MEMIN file %s\n", filename);
        return false;
    }
    
    char line[64];
    int addr = 0;
    while (fgets(line, sizeof(line), fp) && addr < MAIN_MEM_SIZE) {
        // Skip empty lines
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\r' || *p == '\0') continue;
        
        uint32_t data;
        if (sscanf(p, "%x", &data) == 1) {
            sim->main_memory[addr++] = (int32_t)data;
        }
    }
    fclose(fp);
    return true;
}

void write_memout(Simulator* sim, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create %s\n", filename);
        return;
    }
    
    // Find last non-zero address
    int last_addr = 0;
    for (int i = 0; i < MAIN_MEM_SIZE; i++) {
        if (sim->main_memory[i] != 0) last_addr = i;
    }
    
    // Write from 0 to last non-zero (inclusive)
    for (int i = 0; i <= last_addr; i++) {
        fprintf(fp, "%08X\n", (uint32_t)sim->main_memory[i]);
    }
    fclose(fp);
}

void write_regout(Core* core, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create %s\n", filename);
        return;
    }
    
    // Print R2..R15 only (NOT R0 or R1!)
    for (int i = 2; i < NUM_REGISTERS; i++) {
        fprintf(fp, "%08X\n", (uint32_t)core->regs[i]);
    }
    fclose(fp);
}

void write_dsram(Core* core, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create %s\n", filename);
        return;
    }
    
    // 512 words, 8 hex digits each
    for (int i = 0; i < CACHE_SIZE; i++) {
        fprintf(fp, "%08X\n", (uint32_t)core->cache.dsram[i]);
    }
    fclose(fp);
}

void write_tsram(Core* core, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create %s\n", filename);
        return;
    }
    
    // 64 entries
    // TSRAM entry format: tag(12 bits) in upper bits, MESI(2 bits) in lower bits
    // Per spec, output as 8 hex digits for consistency
    for (int i = 0; i < CACHE_NUM_BLOCKS; i++) {
        uint32_t entry = ((core->cache.tsram[i].tag & 0xFFF) << 2) | 
                         (core->cache.tsram[i].mesi & 0x3);
        fprintf(fp, "%08X\n", entry);
    }
    fclose(fp);
}

void write_stats(Core* core, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create %s\n", filename);
        return;
    }
    
    // Exact format from spec (decimal values)
    fprintf(fp, "cycles %u\n", core->cycle_count);
    fprintf(fp, "instructions %u\n", core->instruction_count);
    fprintf(fp, "read_hit %u\n", core->read_hits);
    fprintf(fp, "write_hit %u\n", core->write_hits);
    fprintf(fp, "read_miss %u\n", core->read_misses);
    fprintf(fp, "write_miss %u\n", core->write_misses);
    fprintf(fp, "decode_stall %u\n", core->decode_stall_cycles);
    fprintf(fp, "mem_stall %u\n", core->mem_stall_cycles);
    fclose(fp);
}

/* =============================================================================
 * TRACING
 * =============================================================================
 * 
 * Core trace format per spec:
 * CYCLE FETCH DECODE EXEC MEM WB R2 R3 ... R15
 * 
 * Pipeline stage mapping at START of cycle:
 * - FETCH:  PC of instruction in IF_ID latch
 * - DECODE: PC of instruction in ID_EX latch
 * - EXEC:   PC of instruction in EX_MEM latch
 * - MEM:    PC of instruction in MEM_WB latch
 * - WB:     PC of instruction that completed WB in PREVIOUS cycle
 */

void trace_core(Simulator* sim, int core_id) {
    Core* core = &sim->cores[core_id];
    FILE* fp = sim->core_trace[core_id];
    if (!fp) return;
    
    // Only print if at least one stage is active
    bool any_active = core->IF_ID.valid || core->ID_EX.valid || 
                      core->EX_MEM.valid || core->MEM_WB.valid ||
                      core->WB_completed.valid;
    if (!any_active) return;
    
    fprintf(fp, "%llu ", (unsigned long long)sim->cycle);
    
    // FETCH: instruction in IF_ID
    if (core->IF_ID.valid)
        fprintf(fp, "%03X ", core->IF_ID.pc & PC_MASK);
    else
        fprintf(fp, "--- ");
    
    // DECODE: instruction in ID_EX
    if (core->ID_EX.valid)
        fprintf(fp, "%03X ", core->ID_EX.pc & PC_MASK);
    else
        fprintf(fp, "--- ");
    
    // EXEC: instruction in EX_MEM
    if (core->EX_MEM.valid)
        fprintf(fp, "%03X ", core->EX_MEM.pc & PC_MASK);
    else
        fprintf(fp, "--- ");
    
    // MEM: instruction in MEM_WB
    if (core->MEM_WB.valid)
        fprintf(fp, "%03X ", core->MEM_WB.pc & PC_MASK);
    else
        fprintf(fp, "--- ");
    
    // WB: instruction that completed WB (tracked from previous cycle)
    if (core->WB_completed.valid)
        fprintf(fp, "%03X ", core->WB_completed.pc & PC_MASK);
    else
        fprintf(fp, "--- ");
    
    // Print R2..R15 (8 hex digits each)
    for (int i = 2; i < NUM_REGISTERS; i++) {
        fprintf(fp, "%08X", (uint32_t)core->regs[i]);
        if (i < NUM_REGISTERS - 1) fprintf(fp, " ");
    }
    fprintf(fp, "\n");
}

void trace_bus(Simulator* sim) {
    FILE* fp = sim->bus_trace;
    if (!fp) return;
    
    Bus* bus = &sim->bus;
    
    // Only print if bus_cmd != 0
    if (bus->state.cmd == BUS_CMD_NONE) return;
    
    // Format: CYCLE bus_origid bus_cmd bus_addr bus_data bus_shared
    fprintf(fp, "%llu %X %X %06X %08X %X\n",
            (unsigned long long)sim->cycle,
            bus->state.origid,
            bus->state.cmd,
            bus->state.addr & 0x1FFFFF,  // 21 bits
            (uint32_t)bus->state.data,
            bus->state.shared ? 1 : 0);
}

/* =============================================================================
 * SIMULATION CONTROL
 * =============================================================================
 */

bool pipeline_active(Core* core) {
    return core->IF_ID.valid || core->ID_EX.valid || 
           core->EX_MEM.valid || core->MEM_WB.valid ||
           core->waiting_for_bus;  // Also active if waiting for bus
}

bool all_cores_done(Simulator* sim) {
    for (int i = 0; i < NUM_CORES; i++) {
        Core* core = &sim->cores[i];
        // Core must be halted AND pipeline must be empty
        if (!core->halted || pipeline_active(core)) {
            return false;
        }
    }
    // Also check if bus has pending transaction
    if (sim->bus.arbiter.transaction_in_progress) {
        return false;
    }
    return true;
}

void run_simulation(Simulator* sim) {
    printf("Starting simulation...\n");
    
    // Bootstrap: Pre-fetch first instruction for each core
    // This ensures cycle 1 has the first instruction in IF_ID
    for (int i = 0; i < NUM_CORES; i++) {
        Core* core = &sim->cores[i];
        if (core->pc < IMEM_DEPTH) {
            core->IF_ID.valid = true;
            core->IF_ID.pc = core->pc;
            core->IF_ID.inst = decode_instruction(core->imem[core->pc]);
            core->pc = (core->pc + 1) & PC_MASK;
        }
    }
    
    sim->cycle = 1;  // Start at cycle 1 per trace format
    
    while (!all_cores_done(sim)) {
        // 1. Update per-core cycle counts FIRST (before trace and execution)
        //    Count this cycle for any core that is active (will be traced)
        for (int i = 0; i < NUM_CORES; i++) {
            Core* core = &sim->cores[i];
            if (!core->halted || pipeline_active(core)) {
                core->cycle_count++;
            }
        }
        
        // 2. Trace at beginning of cycle (shows pre-state)
        for (int i = 0; i < NUM_CORES; i++) {
            trace_core(sim, i);
        }
        
        // 3. Bus cycle (arbitration, snoop, memory response)
        bus_cycle(sim);
        
        // 4. Trace bus
        trace_bus(sim);
        
        // 5. Execute all cores
        for (int i = 0; i < NUM_CORES; i++) {
            core_cycle(&sim->cores[i], sim);
        }
        
        // 6. Increment global cycle
        sim->cycle++;
        
        // Safety limit
        if (sim->cycle > 1000000) {
            fprintf(stderr, "Error: Exceeded 1M cycles\n");
            break;
        }
    }
    
    printf("Simulation complete. Total cycles: %llu\n", (unsigned long long)(sim->cycle - 1));
}

/* =============================================================================
 * COMMAND LINE PARSING AND MAIN
 * =============================================================================
 */

int main(int argc, char* argv[]) {
    printf("Multi-Core MESI Simulator\n");
    printf("=========================\n\n");
    
    // Default filenames
    char* imem_files[NUM_CORES] = {"imem0.txt", "imem1.txt", "imem2.txt", "imem3.txt"};
    char* memin_file = "memin.txt";
    char* memout_file = "memout.txt";
    char* regout_files[NUM_CORES] = {"regout0.txt", "regout1.txt", "regout2.txt", "regout3.txt"};
    char* core_trace_files[NUM_CORES] = {"core0trace.txt", "core1trace.txt", "core2trace.txt", "core3trace.txt"};
    char* bus_trace_file = "bustrace.txt";
    char* dsram_files[NUM_CORES] = {"dsram0.txt", "dsram1.txt", "dsram2.txt", "dsram3.txt"};
    char* tsram_files[NUM_CORES] = {"tsram0.txt", "tsram1.txt", "tsram2.txt", "tsram3.txt"};
    char* stats_files[NUM_CORES] = {"stats0.txt", "stats1.txt", "stats2.txt", "stats3.txt"};
    
    // Parse 27 command line arguments if provided
    if (argc == 28) {
        // Order: imem0-3 memin memout regout0-3 core0trace-3trace bustrace dsram0-3 tsram0-3 stats0-3
        int idx = 1;
        for (int i = 0; i < 4; i++) imem_files[i] = argv[idx++];
        memin_file = argv[idx++];
        memout_file = argv[idx++];
        for (int i = 0; i < 4; i++) regout_files[i] = argv[idx++];
        for (int i = 0; i < 4; i++) core_trace_files[i] = argv[idx++];
        bus_trace_file = argv[idx++];
        for (int i = 0; i < 4; i++) dsram_files[i] = argv[idx++];
        for (int i = 0; i < 4; i++) tsram_files[i] = argv[idx++];
        for (int i = 0; i < 4; i++) stats_files[i] = argv[idx++];
    } else if (argc != 1) {
        printf("Usage: %s [imem0 imem1 imem2 imem3 memin memout regout0-3 core0trace-3 bustrace dsram0-3 tsram0-3 stats0-3]\n", argv[0]);
        printf("       (27 arguments total, or no arguments for defaults)\n");
        return 1;
    }
    
    // Initialize
    sim_init(&g_sim);
    
    // Load input files
    for (int i = 0; i < NUM_CORES; i++) {
        load_imem(&g_sim.cores[i], imem_files[i]);
    }
    load_memin(&g_sim, memin_file);
    
    // Open trace files
    for (int i = 0; i < NUM_CORES; i++) {
        g_sim.core_trace[i] = fopen(core_trace_files[i], "w");
    }
    g_sim.bus_trace = fopen(bus_trace_file, "w");
    
    // Run simulation
    run_simulation(&g_sim);
    
    // Close trace files
    for (int i = 0; i < NUM_CORES; i++) {
        if (g_sim.core_trace[i]) {
            fclose(g_sim.core_trace[i]);
            g_sim.core_trace[i] = NULL;
        }
    }
    if (g_sim.bus_trace) {
        fclose(g_sim.bus_trace);
        g_sim.bus_trace = NULL;
    }
    
    // Flush all dirty cache lines to main memory before writing memout
    for (int c = 0; c < NUM_CORES; c++) {
        for (int line = 0; line < CACHE_NUM_BLOCKS; line++) {
            cache_writeback_block(&g_sim.cores[c], &g_sim, line);
        }
    }
    
    // Write output files
    write_memout(&g_sim, memout_file);
    for (int i = 0; i < NUM_CORES; i++) {
        write_regout(&g_sim.cores[i], regout_files[i]);
        write_dsram(&g_sim.cores[i], dsram_files[i]);
        write_tsram(&g_sim.cores[i], tsram_files[i]);
        write_stats(&g_sim.cores[i], stats_files[i]);
    }
    
    printf("All output files written.\n");
    
    // Cleanup
    sim_cleanup(&g_sim);
    
    return 0;
}
