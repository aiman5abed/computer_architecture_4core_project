/*
 * =============================================================================
 * Pipeline Implementation
 * =============================================================================
 * 5-Stage Pipeline: Fetch -> Decode -> Execute -> Mem -> Writeback
 * 
 * KEY RULES FROM SPEC:
 * - Branch resolution in DECODE stage
 * - Delay slot: instruction after branch ALWAYS executes
 * - NO forwarding: register reads do NOT see same-cycle writes
 * - R0 is hardwired to 0 (writes ignored)
 * - R1 = sign-extended immediate of currently decoded instruction
 * - Data hazards: stall in DECODE
 * - Cache miss: stall in MEM
 * =============================================================================
 */

#include "../include/sim.h"

/* =============================================================================
 * HAZARD DETECTION
 * =============================================================================
 */

// Get destination register for an instruction (if any)
int get_dest_reg(PipelineLatch* latch) {
    if (!latch->valid) return -1;
    
    uint8_t op = latch->inst.opcode;
    
    // ALU ops write to rd
    if (op <= OP_SRL) {
        return latch->inst.rd;
    }
    
    // LW writes to rd
    if (op == OP_LW) {
        return latch->inst.rd;
    }
    
    // JAL writes to R15
    if (op == OP_JAL) {
        return 15;
    }
    
    // Branches, SW, HALT don't write
    return -1;
}

// Check if instruction in latch writes to a register
bool is_reg_written_by(PipelineLatch* latch) {
    return get_dest_reg(latch) > 0;  // >0 to exclude R0
}

// Check if a register is being written by an instruction still in pipeline
bool is_reg_in_flight(Core* core, int reg) {
    // R0 is never a hazard (hardwired to 0)
    // R1 is special (immediate) - not a hazard from normal writes
    if (reg <= 1) return false;
    
    // Check ID_EX (will write in future)
    if (get_dest_reg(&core->ID_EX) == reg) return true;
    
    // Check EX_MEM (will write in future)
    if (get_dest_reg(&core->EX_MEM) == reg) return true;
    
    // Check MEM_WB (will write this cycle, but not visible until next)
    if (get_dest_reg(&core->MEM_WB) == reg) return true;
    
    return false;
}

// Check for data hazards requiring decode stall
bool check_data_hazard(Core* core) {
    if (!core->IF_ID.valid) return false;
    
    Instruction* inst = &core->IF_ID.inst;
    
    // Check rs dependency
    if (is_reg_in_flight(core, inst->rs)) return true;
    
    // Check rt dependency  
    if (is_reg_in_flight(core, inst->rt)) return true;
    
    // For branches and JAL, also check rd (branch target)
    if (inst->opcode >= OP_BEQ && inst->opcode <= OP_JAL) {
        if (is_reg_in_flight(core, inst->rd)) return true;
    }
    
    // For SW, check rd (data to store)
    if (inst->opcode == OP_SW) {
        if (is_reg_in_flight(core, inst->rd)) return true;
    }
    
    return false;
}

/* =============================================================================
 * PIPELINE STAGES
 * =============================================================================
 */

// Fetch stage
static void do_fetch(Core* core, PipelineLatch* next_IF_ID) {
    if (core->halted) {
        next_IF_ID->valid = false;
        return;
    }
    
    if (core->decode_stall || core->mem_stall) {
        // Stalled - keep current IF_ID
        *next_IF_ID = core->IF_ID;
        return;
    }
    
    // Fetch from IMEM
    if (core->pc < IMEM_DEPTH) {
        next_IF_ID->valid = true;
        next_IF_ID->pc = core->pc;
        next_IF_ID->inst = decode_instruction(core->imem[core->pc]);
        core->pc++;
    } else {
        next_IF_ID->valid = false;
    }
}

// Decode stage
static void do_decode(Core* core, Simulator* sim, PipelineLatch* next_ID_EX, bool* branch_taken, uint32_t* branch_target) {
    *branch_taken = false;
    
    if (core->mem_stall) {
        // MEM stall freezes everything
        *next_ID_EX = core->ID_EX;
        return;
    }
    
    if (!core->IF_ID.valid) {
        next_ID_EX->valid = false;
        return;
    }

    Instruction* inst = &core->IF_ID.inst;

    // --- FIX: Update R1 ALWAYS, even if we are about to stall ---
    // R1 reflects the immediate of the instruction CURRENTLY in Decode
    core->regs[1] = inst->immediate;
    
    // Check data hazard
    if (check_data_hazard(core)) {
        core->decode_stall = true;
        core->decode_stall_cycles++;
        // Insert bubble (NOP) into ID_EX
        next_ID_EX->valid = false;
        return;
    }
    
    core->decode_stall = false;
    
    // Move IF_ID to ID_EX
    *next_ID_EX = core->IF_ID;
    
    // Note: 'inst' pointer is valid because it points to core->IF_ID which hasn't changed
    
    // Read register values
    next_ID_EX->rs_val = core->regs[inst->rs];
    next_ID_EX->rt_val = core->regs[inst->rt];
    next_ID_EX->rd_val = core->regs[inst->rd];
    
    // Branch resolution logic...
    // (Rest of your function is fine)
    int32_t rs = next_ID_EX->rs_val;
    int32_t rt = next_ID_EX->rt_val;
    uint32_t target = next_ID_EX->rd_val & PC_MASK;
    
    switch (inst->opcode) {
        case OP_BEQ: *branch_taken = (rs == rt); break;
        case OP_BNE: *branch_taken = (rs != rt); break;
        case OP_BLT: *branch_taken = (rs < rt); break;
        case OP_BGT: *branch_taken = (rs > rt); break;
        case OP_BLE: *branch_taken = (rs <= rt); break;
        case OP_BGE: *branch_taken = (rs >= rt); break;
        case OP_JAL:
            *branch_taken = true;
            next_ID_EX->alu_result = core->pc; 
            break;
        case OP_HALT:
            break;
        default:
            break;
    }
    
    if (*branch_taken) {
        *branch_target = target;
    }
}

// Execute stage
static void do_execute(Core* core, PipelineLatch* next_EX_MEM) {
    if (core->mem_stall) {
        *next_EX_MEM = core->EX_MEM;
        return;
    }
    
    if (!core->ID_EX.valid) {
        next_EX_MEM->valid = false;
        return;
    }
    
    *next_EX_MEM = core->ID_EX;
    
    Instruction* inst = &next_EX_MEM->inst;
    int32_t rs = next_EX_MEM->rs_val;
    int32_t rt = next_EX_MEM->rt_val;
    int32_t result = 0;
    
    switch (inst->opcode) {
        case OP_ADD: result = rs + rt; break;
        case OP_SUB: result = rs - rt; break;
        case OP_AND: result = rs & rt; break;
        case OP_OR:  result = rs | rt; break;
        case OP_XOR: result = rs ^ rt; break;
        case OP_MUL: result = rs * rt; break;
        case OP_SLL: result = (int32_t)((uint32_t)rs << (rt & 0x1F)); break;
        case OP_SRA: result = rs >> (rt & 0x1F); break;
        case OP_SRL: result = (int32_t)((uint32_t)rs >> (rt & 0x1F)); break;
        case OP_LW:
        case OP_SW:
            // Effective address
            result = rs + rt;
            break;
        case OP_JAL:
            // Keep return address from decode
            result = next_EX_MEM->alu_result;
            break;
        default:
            break;
    }
    
    next_EX_MEM->alu_result = result;
}

// Memory stage
static void do_mem(Core* core, Simulator* sim, PipelineLatch* next_MEM_WB) {
    if (!core->EX_MEM.valid) {
        next_MEM_WB->valid = false;
        core->mem_stall = false;
        return;
    }
    
    *next_MEM_WB = core->EX_MEM;
    
    Instruction* inst = &next_MEM_WB->inst;
    uint32_t addr = (uint32_t)next_MEM_WB->alu_result;
    
    if (inst->opcode == OP_LW) {
        int32_t data;
        if (cache_read(core, sim, addr, &data)) {
            next_MEM_WB->mem_data = data;
            core->mem_stall = false;
        } else {
            // Cache miss - stall
            core->mem_stall = true;
            core->mem_stall_cycles++;
            // Keep EX_MEM, don't advance
            next_MEM_WB->valid = false;
        }
    } else if (inst->opcode == OP_SW) {
        int32_t data = next_MEM_WB->rd_val;
        if (cache_write(core, sim, addr, data)) {
            core->mem_stall = false;
        } else {
            // Cache miss - stall
            core->mem_stall = true;
            core->mem_stall_cycles++;
            next_MEM_WB->valid = false;
        }
    } else {
        core->mem_stall = false;
    }
}

// Writeback stage
static void do_writeback(Core* core) {
    if (!core->MEM_WB.valid) return;
    
    Instruction* inst = &core->MEM_WB.inst;
    int rd = inst->rd;
    
    // Check for HALT
    if (inst->opcode == OP_HALT) {
        core->halted = true;
        core->instruction_count++;
        return;
    }
    
    // Determine what to write
    int32_t value = 0;
    bool do_write = false;
    int dest = rd;
    
    switch (inst->opcode) {
        case OP_ADD: case OP_SUB: case OP_AND: case OP_OR:
        case OP_XOR: case OP_MUL: case OP_SLL: case OP_SRA: case OP_SRL:
            value = core->MEM_WB.alu_result;
            do_write = true;
            break;
        case OP_LW:
            value = core->MEM_WB.mem_data;
            do_write = true;
            break;
        case OP_JAL:
            value = core->MEM_WB.alu_result;  // Return address
            dest = 15;  // JAL always writes to R15
            do_write = true;
            break;
        default:
            break;
    }
    
    // Write to register (R0 writes ignored, R1 writes ignored)
    if (do_write && dest >= 2) {
        core->regs[dest] = value;
    }
    
    core->instruction_count++;
}

/* =============================================================================
 * CORE CYCLE
 * =============================================================================
 */

void core_cycle(Core* core, Simulator* sim) {
    // Next state latches
    PipelineLatch next_IF_ID = {0};
    PipelineLatch next_ID_EX = {0};
    PipelineLatch next_EX_MEM = {0};
    PipelineLatch next_MEM_WB = {0};
    
    bool branch_taken = false;
    uint32_t branch_target = 0;
    
    // Save what's in MEM_WB for WB_out tracking (what will complete WB this cycle)
    PipelineLatch completing_wb = core->MEM_WB;
    
    // Execute stages (conceptually in parallel, but we compute next state)
    
    // WB (commits this cycle)
    do_writeback(core);
    
    // MEM -> MEM_WB
    do_mem(core, sim, &next_MEM_WB);
    
    // EX -> EX_MEM (if not stalled)
    do_execute(core, &next_EX_MEM);
    
    // ID -> ID_EX (if not stalled), also resolves branches
    do_decode(core, sim, &next_ID_EX, &branch_taken, &branch_target);
    
    // IF -> IF_ID (if not stalled)
    do_fetch(core, &next_IF_ID);
    
    // Handle branch (with delay slot)
    // The instruction currently being fetched (delay slot) still executes
    if (branch_taken && !core->decode_stall && !core->mem_stall) {
        core->pc = branch_target;
        // Note: next_IF_ID already has the delay slot instruction
    }
    
    // Update latches (if not stalled in MEM)
    if (!core->mem_stall) {
        core->WB_out = completing_wb;  // Track what completed WB this cycle
        core->MEM_WB = next_MEM_WB;
        core->EX_MEM = next_EX_MEM;
        
        if (!core->decode_stall) {
            core->ID_EX = next_ID_EX;
            core->IF_ID = next_IF_ID;
        } else {
            // Decode stall: bubble in ID_EX, keep IF_ID
            core->ID_EX.valid = false;
        }
    } else {
        // Mem stall - WB still happened, so track it
        core->WB_out = completing_wb;
    }
    // If mem_stall, all latches frozen (handled in individual stages)
}
