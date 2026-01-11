/*
 * =============================================================================
 * Cache and MESI Protocol Implementation
 * =============================================================================
 * 
 * Cache Organization (from PDF spec):
 * - Direct-mapped
 * - Total: 512 words (DSRAM)
 * - Block size: 8 words
 * - 64 cache lines (TSRAM entries)
 * - Write-back + write-allocate
 * - Hit latency: 1 cycle
 * 
 * Address breakdown (21-bit word address):
 * - Offset: 3 bits (word within 8-word block)
 * - Index:  6 bits (0-63 cache line)
 * - Tag:   12 bits
 * 
 * MESI States: 0=Invalid, 1=Shared, 2=Exclusive, 3=Modified
 * =============================================================================
 */

#include "sim.h"

/* =============================================================================
 * ADDRESS DECOMPOSITION
 * =============================================================================
 */

// Get word offset within block (3 bits, 0-7)
int cache_get_offset(uint32_t addr) {
    return addr & 0x7;  // Lower 3 bits
}

// Get cache line index (6 bits, 0-63)
int cache_get_index(uint32_t addr) {
    return (addr >> BLOCK_OFFSET_BITS) & 0x3F;
}

// Get tag (12 bits)
uint32_t cache_get_tag(uint32_t addr) {
    return addr >> (BLOCK_OFFSET_BITS + INDEX_BITS);
}

// Get block-aligned address (clear offset bits)
uint32_t cache_get_block_addr(uint32_t addr) {
    return addr & ~((uint32_t)0x7);  // Clear lower 3 bits
}

// DSRAM address from index and offset
static int dsram_addr(int index, int offset) {
    return (index << BLOCK_OFFSET_BITS) | offset;
}

/* =============================================================================
 * CACHE HIT CHECK
 * =============================================================================
 */

static bool cache_hit_check(Cache* cache, uint32_t addr) {
    int index = cache_get_index(addr);
    uint32_t tag = cache_get_tag(addr);
    TSRAMEntry* entry = &cache->tsram[index];
    
    return (entry->mesi != MESI_INVALID) && (entry->tag == tag);
}

/* =============================================================================
 * CACHE READ
 * =============================================================================
 * Returns true if hit (data valid), false if miss (stall required)
 */

bool cache_read(Core* core, Simulator* sim, uint32_t addr, int32_t* data) {
    Cache* cache = &core->cache;
    int index = cache_get_index(addr);
    int offset = cache_get_offset(addr);
    uint32_t tag = cache_get_tag(addr);
    TSRAMEntry* entry = &cache->tsram[index];
    
    // Check hit
    if (entry->mesi != MESI_INVALID && entry->tag == tag) {
        // Hit!
        *data = cache->dsram[dsram_addr(index, offset)];
        core->read_hits++;
        return true;
    }
    
    // Miss - only count miss once per request (when starting bus transaction)
    if (!core->waiting_for_bus && !core->bus_request_pending) {
        core->read_misses++;
        uint32_t block_addr = cache_get_block_addr(addr);
        
        // Check if eviction needed (current line is Modified with different tag)
        if (entry->mesi == MESI_MODIFIED && entry->tag != tag) {
            // Writeback will happen when we get bus access
        }
        
        // Issue BusRd
        bus_issue_request(core, BUS_CMD_BUSRD, block_addr);
    }
    
    return false;  // Stall
}

/* =============================================================================
 * CACHE WRITE
 * =============================================================================
 * Returns true if completed, false if stall required
 */

bool cache_write(Core* core, Simulator* sim, uint32_t addr, int32_t data) {
    Cache* cache = &core->cache;
    int index = cache_get_index(addr);
    int offset = cache_get_offset(addr);
    uint32_t tag = cache_get_tag(addr);
    TSRAMEntry* entry = &cache->tsram[index];
    
    // Check hit
    if (entry->mesi != MESI_INVALID && entry->tag == tag) {
        // Hit
        if (entry->mesi == MESI_MODIFIED || entry->mesi == MESI_EXCLUSIVE) {
            // Can write directly, transition to M
            cache->dsram[dsram_addr(index, offset)] = data;
            entry->mesi = MESI_MODIFIED;
            core->write_hits++;
            return true;
        } else if (entry->mesi == MESI_SHARED) {
            // Need to upgrade to exclusive (BusRdX)
            // This counts as a write miss (upgrade miss)
            if (!core->waiting_for_bus && !core->bus_request_pending) {
                core->write_misses++;
                uint32_t block_addr = cache_get_block_addr(addr);
                core->pending_write_data = data;
                core->pending_store_addr = addr;
                core->pending_is_write = true;
                bus_issue_request(core, BUS_CMD_BUSRDX, block_addr);
            }
            return false;  // Stall
        }
    }
    
    // Miss - write-allocate policy, need to fetch block first
    if (!core->waiting_for_bus && !core->bus_request_pending) {
        core->write_misses++;
        uint32_t block_addr = cache_get_block_addr(addr);
        core->pending_write_data = data;
        core->pending_store_addr = addr;
        core->pending_is_write = true;
        bus_issue_request(core, BUS_CMD_BUSRDX, block_addr);
    }
    
    return false;  // Stall
}

/* =============================================================================
 * CACHE WRITEBACK (for eviction)
 * =============================================================================
 */

void cache_writeback_block(Core* core, Simulator* sim, int index) {
    Cache* cache = &core->cache;
    TSRAMEntry* entry = &cache->tsram[index];
    
    if (entry->mesi != MESI_MODIFIED) return;
    
    // Calculate block address from tag and index
    uint32_t block_addr = (entry->tag << (INDEX_BITS + BLOCK_OFFSET_BITS)) |
                          ((uint32_t)index << BLOCK_OFFSET_BITS);
    
    // Write all 8 words to main memory
    for (int i = 0; i < CACHE_BLOCK_SIZE; i++) {
        sim->main_memory[block_addr + i] = cache->dsram[dsram_addr(index, i)];
    }
}

/* =============================================================================
 * BUS REQUEST ISSUE
 * =============================================================================
 */

void bus_issue_request(Core* core, BusCommand cmd, uint32_t block_addr) {
    core->bus_request_pending = true;
    core->pending_bus_cmd = cmd;
    core->pending_bus_addr = block_addr;
    core->waiting_for_bus = true;
}

/* =============================================================================
 * MESI SNOOP HANDLERS
 * =============================================================================
 */

// Called when another core issues BusRd
void mesi_snoop_busrd(Core* core, Simulator* sim, uint32_t block_addr, int requester) {
    if (core->core_id == requester) return;
    
    Cache* cache = &core->cache;
    int index = cache_get_index(block_addr);
    uint32_t tag = cache_get_tag(block_addr);
    TSRAMEntry* entry = &cache->tsram[index];
    
    // Check if we have this block
    if (entry->mesi == MESI_INVALID || entry->tag != tag) {
        return;  // Don't have it
    }
    
    // We have it - set shared signal
    sim->bus.snoop_shared = true;
    
    switch (entry->mesi) {
        case MESI_MODIFIED:
            // We have dirty data - we'll supply it
            sim->bus.snoop_has_modified = true;
            sim->bus.snoop_modified_core = core->core_id;
            // Transition M -> S (will happen after data transfer)
            entry->mesi = MESI_SHARED;
            break;
            
        case MESI_EXCLUSIVE:
            // Transition E -> S
            entry->mesi = MESI_SHARED;
            break;
            
        case MESI_SHARED:
            // Stay S
            break;
            
        default:
            break;
    }
}

// Called when another core issues BusRdX
void mesi_snoop_busrdx(Core* core, Simulator* sim, uint32_t block_addr, int requester) {
    if (core->core_id == requester) return;
    
    Cache* cache = &core->cache;
    int index = cache_get_index(block_addr);
    uint32_t tag = cache_get_tag(block_addr);
    TSRAMEntry* entry = &cache->tsram[index];
    
    // Check if we have this block
    if (entry->mesi == MESI_INVALID || entry->tag != tag) {
        return;  // Don't have it
    }
    
    // We have it
    if (entry->mesi == MESI_MODIFIED) {
        // We have dirty data - need to supply it before invalidating
        sim->bus.snoop_has_modified = true;
        sim->bus.snoop_modified_core = core->core_id;
    }
    
    // Invalidate our copy
    entry->mesi = MESI_INVALID;
}
