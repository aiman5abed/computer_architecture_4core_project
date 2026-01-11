/*
 * =============================================================================
 * Bus and Memory Controller Implementation
 * =============================================================================
 * 
 * Bus Signals (from PDF spec):
 * - bus_origid: 3 bits (0-3 = core, 4 = memory)
 * - bus_cmd: 2 bits (0=none, 1=BusRd, 2=BusRdX, 3=Flush)
 * - bus_addr: 21-bit word address
 * - bus_data: 32-bit data word
 * - bus_shared: 1-bit, set by snooping caches
 * 
 * Protocol Rules:
 * - Only ONE transaction per cycle
 * - Fair round-robin arbitration (last winner = lowest priority next)
 * - Do not grant bus if busy OR if BusRd/BusRdX not yet completed via Flush
 * 
 * Memory Response Timing:
 * - After BusRd/BusRdX: 16 cycles delay
 * - Then: 8 consecutive Flush cycles (one word per cycle)
 * - If another cache has M state, that cache supplies data via Flush
 *   and memory updates in parallel
 * =============================================================================
 */

#include "sim.h"

/* =============================================================================
 * BUS ARBITRATION
 * =============================================================================
 */

// Round-robin arbitration
// Returns core ID to grant, or -1 if no requests or bus busy
int bus_arbitrate(Simulator* sim) {
    Bus* bus = &sim->bus;
    
    // Cannot grant if transaction in progress (waiting for Flush completion)
    if (bus->arbiter.transaction_in_progress) {
        return -1;
    }
    
    // Round-robin: start from core after last granted
    for (int i = 0; i < NUM_CORES; i++) {
        int core_id = (bus->arbiter.last_granted + 1 + i) % NUM_CORES;
        Core* core = &sim->cores[core_id];
        
        if (core->bus_request_pending) {
            return core_id;
        }
    }
    
    return -1;
}

/* =============================================================================
 * BUS SNOOP
 * =============================================================================
 */

void bus_snoop(Simulator* sim, BusCommand cmd, uint32_t addr, int requester) {
    // Reset snoop results
    sim->bus.snoop_shared = false;
    sim->bus.snoop_has_modified = false;
    sim->bus.snoop_modified_core = -1;
    
    // All other caches snoop
    for (int i = 0; i < NUM_CORES; i++) {
        if (i == requester) continue;
        
        if (cmd == BUS_CMD_BUSRD) {
            mesi_snoop_busrd(&sim->cores[i], sim, addr, requester);
        } else if (cmd == BUS_CMD_BUSRDX) {
            mesi_snoop_busrdx(&sim->cores[i], sim, addr, requester);
        }
    }
}

/* =============================================================================
 * MEMORY RESPONSE
 * =============================================================================
 */

// Start memory response (called when BusRd/BusRdX is granted)
static void memory_start_response(Simulator* sim, int core_id, uint32_t block_addr, 
                                   bool is_rdx, int data_source, bool shared) {
    MemoryResponse* resp = &sim->bus.mem_response;
    
    resp->valid = true;
    resp->requesting_core = core_id;
    resp->block_addr = block_addr;
    resp->is_rdx = is_rdx;
    resp->data_source = data_source;
    resp->words_sent = 0;
    resp->shared = shared;
    
    // 16 cycle delay before first word
    resp->cycles_remaining = MEM_RESPONSE_DELAY;
}

// Send one Flush word
static void memory_send_flush(Simulator* sim) {
    Bus* bus = &sim->bus;
    MemoryResponse* resp = &bus->mem_response;
    
    if (!resp->valid || resp->words_sent >= CACHE_BLOCK_SIZE) {
        return;
    }
    
    uint32_t word_addr = resp->block_addr + resp->words_sent;
    int32_t data;
    int origid;
    
    // Get data from source
    if (resp->data_source >= 0 && resp->data_source < NUM_CORES) {
        // Data from cache with M state
        Core* src_core = &sim->cores[resp->data_source];
        int src_index = cache_get_index(resp->block_addr);
        int src_offset = resp->words_sent;
        data = src_core->cache.dsram[(src_index << BLOCK_OFFSET_BITS) | src_offset];
        origid = resp->data_source;
        
        // Also update main memory (write-back)
        sim->main_memory[word_addr] = data;
    } else {
        // Data from main memory
        data = sim->main_memory[word_addr];
        origid = BUS_ORIG_MEMORY;
    }
    
    // Set bus state for Flush
    bus->state.cmd = BUS_CMD_FLUSH;
    bus->state.origid = origid;
    bus->state.addr = word_addr;
    bus->state.data = data;
    bus->state.shared = resp->shared;
    bus->state.active = true;
    
    // Write data to requesting core's cache
    Core* req_core = &sim->cores[resp->requesting_core];
    int req_index = cache_get_index(resp->block_addr);
    int req_offset = resp->words_sent;
    req_core->cache.dsram[(req_index << BLOCK_OFFSET_BITS) | req_offset] = data;
    
    resp->words_sent++;
    
    // Check if done
    if (resp->words_sent >= CACHE_BLOCK_SIZE) {
        // Update requesting cache's TSRAM
        TSRAMEntry* entry = &req_core->cache.tsram[req_index];
        entry->tag = cache_get_tag(resp->block_addr);
        
        if (resp->is_rdx) {
            entry->mesi = MESI_MODIFIED;
            
            // If this was a write miss, complete the pending write
            if (req_core->pending_is_write) {
                int offset = cache_get_offset(req_core->pending_store_addr);
                req_core->cache.dsram[(req_index << BLOCK_OFFSET_BITS) | offset] = 
                    req_core->pending_write_data;
                req_core->pending_is_write = false;
            }
        } else {
            // BusRd - state depends on bus_shared
            if (resp->shared) {
                entry->mesi = MESI_SHARED;
            } else {
                entry->mesi = MESI_EXCLUSIVE;
            }
        }
        
        // Clear core's waiting state
        req_core->waiting_for_bus = false;
        req_core->bus_request_pending = false;
        req_core->mem_stall = false;
        
        // Transaction complete
        resp->valid = false;
        bus->arbiter.transaction_in_progress = false;
    }
}

/* =============================================================================
 * MEMORY CYCLE
 * =============================================================================
 */

void memory_cycle(Simulator* sim) {
    MemoryResponse* resp = &sim->bus.mem_response;
    
    if (!resp->valid) return;
    
    // Countdown delay
    if (resp->cycles_remaining > 0) {
        resp->cycles_remaining--;
        // Bus shows no activity during countdown
        sim->bus.state.cmd = BUS_CMD_NONE;
        sim->bus.state.active = false;
        return;
    }
    
    // Send next Flush word
    memory_send_flush(sim);
}

/* =============================================================================
 * BUS CYCLE
 * =============================================================================
 */

void bus_cycle(Simulator* sim) {
    Bus* bus = &sim->bus;
    
    // Clear bus state at start of cycle
    bus->state.cmd = BUS_CMD_NONE;
    bus->state.data = 0;
    bus->state.active = false;
    
    // If transaction in progress (waiting for Flush), handle memory response
    if (bus->arbiter.transaction_in_progress) {
        memory_cycle(sim);
        return;
    }
    
    // Arbitrate for new transaction
    int granted = bus_arbitrate(sim);
    
    if (granted < 0) {
        return;  // No requests
    }
    
    Core* core = &sim->cores[granted];
    BusCommand cmd = core->pending_bus_cmd;
    uint32_t addr = core->pending_bus_addr;
    
    // Before issuing, handle eviction if needed
    int index = cache_get_index(addr);
    TSRAMEntry* entry = &core->cache.tsram[index];
    if (entry->mesi == MESI_MODIFIED && entry->tag != cache_get_tag(addr)) {
        // Need to writeback old block first
        cache_writeback_block(core, sim, index);
    }
    
    // Perform snooping BEFORE setting bus state (to get shared signal)
    bus_snoop(sim, cmd, addr, granted);
    
    // Issue the transaction
    bus->state.active = true;
    bus->state.cmd = cmd;
    bus->state.origid = granted;
    bus->state.addr = addr;
    bus->state.data = 0;
    bus->state.shared = bus->snoop_shared;  // Set from snoop results
    
    // Update arbiter
    bus->arbiter.last_granted = granted;
    bus->arbiter.transaction_in_progress = true;
    
    // Clear core's pending request flag (but keep waiting_for_bus true)
    core->bus_request_pending = false;
    
    // Start memory response
    int data_source = bus->snoop_has_modified ? bus->snoop_modified_core : -1;
    memory_start_response(sim, granted, addr, (cmd == BUS_CMD_BUSRDX), 
                          data_source, bus->snoop_shared);
}
