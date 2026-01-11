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
 * - Register write in cycle N visible in cycle N+1
 * - R0 is hardwired to 0 (writes ignored)
 * - R1 = sign-extended immediate of currently decoded instruction
 * - Data hazards: stall in DECODE
 * - Cache miss: stall in MEM
 * =============================================================================
 */

#include "sim.h"

/* =============================================================================
 * HAZARD DETECTION
 * =============================================================================
 */

// Get destination register for an instruction (if any)
// Returns -1 if no destination
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

// Check if a register is being written by an instruction still in pipeline
// (and the write hasn't completed yet)
static bool is_reg_in_flight(Core* core, int reg) {
    // R0 is never a hazard (hardwired to 0)
    // R1 is special (immediate) - not a hazard from normal writes
    if (reg <= 1) return false;
    
    // Check ID_EX (will write in future)
    if (get_dest_reg(&core->ID_EX) == reg) return true;
    
    // Check EX_MEM (will write in future)
    if (get_dest_reg(&core->EX_MEM) == reg) return true;
    
    // Check MEM_WB (will write this cycle, but not visible until next cycle)
    if (get_dest_reg(&core->MEM_WB) == reg) return true;
    
    return false;
}

// Check for data hazards requiring decode stall
bool check_data_hazard(Core* core) {
    if (!core->IF_ID.valid) return false;
    
    Instruction* inst = &core->IF_ID.inst;
    
    // Check rs dependency (used by most instructions)
    if (inst->rs != 0 && inst->rs != 1) {
        if (is_reg_in_flight(core, inst->rs)) return true;
    }
    
    // Check rt dependency (used by most instructions)
    if (inst->rt != 0 && inst->rt != 1) {
        if (is_reg_in_flight(core, inst->rt)) return true;
    }
    
    // For branches and JAL, also check rd (branch target is in rd)
    if (inst->opcode >= OP_BEQ && inst->opcode <= OP_JAL) {
        if (inst->rd != 0 && inst->rd != 1) {
            if (is_reg_in_flight(core, inst->rd)) return true;
        }
    }
    
    // For SW, check rd (data to store)
    if (inst->opcode == OP_SW) {
        if (inst->rd != 0 && inst->rd != 1) {
            if (is_reg_in_flight(core, inst->rd)) return true;
        }
    }
    
    return false;
}

/* =============================================================================
 * PIPELINE STAGES
 * =============================================================================
 */

// Writeback stage - commits to register file
// Returns true if instruction completed (for instruction count)
static bool do_writeback(Core* core) {
    if (!core->MEM_WB.valid) return false;
    
    Instruction* inst = &core->MEM_WB.inst;
    
    // Check for HALT
    if (inst->opcode == OP_HALT) {
        core->halted = true;
        return true;
    }
    
    // Determine what to write
    int32_t value = 0;
    bool do_write = false;
    int dest = inst->rd;
    
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
    
    // Write to register (R0 and R1 writes ignored)
    if (do_write && dest >= 2) {
        core->regs[dest] = value;
    }
    
    return true;  // Instruction completed
}

// Memory stage
// Returns true if completed without stall
static bool do_mem(Core* core, Simulator* sim, PipelineLatch* next_MEM_WB) {
    if (!core->EX_MEM.valid) {
        next_MEM_WB->valid = false;
        return true;
    }
    
    // Copy from EX_MEM
    *next_MEM_WB = core->EX_MEM;
    
    Instruction* inst = &next_MEM_WB->inst;
    uint32_t addr = (uint32_t)next_MEM_WB->alu_result & 0x1FFFFF;  // 21-bit address
    
    if (inst->opcode == OP_LW) {
        int32_t data;
        if (cache_read(core, sim, addr, &data)) {
            next_MEM_WB->mem_data = data;
            return true;
        } else {
            // Cache miss - stall
            next_MEM_WB->valid = false;
            return false;
        }
    } else if (inst->opcode == OP_SW) {
        int32_t data = next_MEM_WB->rd_val;
        if (cache_write(core, sim, addr, data)) {
            return true;
        } else {
            // Cache miss - stall
            next_MEM_WB->valid = false;
            return false;
        }
    }
    
    return true;
}

// Execute stage
static void do_execute(Core* core, PipelineLatch* next_EX_MEM) {
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
        case OP_SRA: result = rs >> (rt & 0x1F); break;  // Arithmetic shift
        case OP_SRL: result = (int32_t)((uint32_t)rs >> (rt & 0x1F)); break;
        case OP_LW:
        case OP_SW:
            // Effective address = rs + rt (word address)
            result = rs + rt;
            break;
        case OP_JAL:
            // Return address already computed in decode
            result = next_EX_MEM->alu_result;
            break;
        default:
            break;
    }
    
    next_EX_MEM->alu_result = result;
}

// Decode stage - also resolves branches
// Returns true if branch taken, target in branch_target
static bool do_decode(Core* core, Simulator* sim, PipelineLatch* next_ID_EX, 
                      uint32_t* branch_target) {
    bool branch_taken = false;
    *branch_target = 0;
    
    if (!core->IF_ID.valid) {
        next_ID_EX->valid = false;
        return false;
    }
    
    // Check data hazard
    if (check_data_hazard(core)) {
        core->decode_stall = true;
        core->decode_stall_cycles++;
        // Insert bubble (NOP) into ID_EX
        next_ID_EX->valid = false;
        return false;
    }
    
    core->decode_stall = false;
    
    // Move IF_ID to ID_EX
    *next_ID_EX = core->IF_ID;
    
    Instruction* inst = &next_ID_EX->inst;
    
    // Update R1 with sign-extended immediate (per spec, for EVERY instruction decoded)
    core->regs[1] = inst->immediate;
    
    // Read register values (using current register values)
    // R0 always reads as 0, R1 reads as immediate (just updated above)
    next_ID_EX->rs_val = (inst->rs == 0) ? 0 : core->regs[inst->rs];
    next_ID_EX->rt_val = (inst->rt == 0) ? 0 : core->regs[inst->rt];
    next_ID_EX->rd_val = (inst->rd == 0) ? 0 : core->regs[inst->rd];
    
    // Branch resolution in decode
    int32_t rs = next_ID_EX->rs_val;
    int32_t rt = next_ID_EX->rt_val;
    uint32_t target = next_ID_EX->rd_val & PC_MASK;  // 10-bit target from rd
    
    switch (inst->opcode) {
        case OP_BEQ: branch_taken = (rs == rt); break;
        case OP_BNE: branch_taken = (rs != rt); break;
        case OP_BLT: branch_taken = (rs < rt); break;
        case OP_BGT: branch_taken = (rs > rt); break;
        case OP_BLE: branch_taken = (rs <= rt); break;
        case OP_BGE: branch_taken = (rs >= rt); break;
        case OP_JAL:
            branch_taken = true;
            // Store return address (PC of instruction AFTER delay slot)
            // At this point, PC points to instruction after IF_ID (the delay slot)
            // So return address = PC (which is delay slot + 1 from branch's view)
            // Actually, JAL stores PC+1 where PC is the JAL's PC
            // Since IF_ID.pc is JAL's PC, return address = IF_ID.pc + 1
            // But delay slot executes, so it's actually IF_ID.pc + 2? No...
            // Per spec: JAL stores PC+1 in R15, where PC is JAL's address
            next_ID_EX->alu_result = (core->IF_ID.pc + 1) & PC_MASK;
            break;
        default:
            break;
    }
    
    if (branch_taken) {
        *branch_target = target;
    }
    
    return branch_taken;
}

// Fetch stage
static void do_fetch(Core* core, PipelineLatch* next_IF_ID) {
    if (core->halted || !core->fetch_enabled) {
        next_IF_ID->valid = false;
        return;
    }
    
    // Fetch from IMEM
    if (core->pc < IMEM_DEPTH) {
        next_IF_ID->valid = true;
        next_IF_ID->pc = core->pc;
        next_IF_ID->inst = decode_instruction(core->imem[core->pc]);
    } else {
        next_IF_ID->valid = false;
    }
}

/* =============================================================================
 * CORE CYCLE
 * =============================================================================
 * 
 * Pipeline timing (5 stages, in-order):
 * - All stages conceptually operate in parallel
 * - We compute next state, then update at end of cycle
 * - Register writes in WB visible next cycle
 * - Branch resolved in Decode, delay slot always executes
 */

void core_cycle(Core* core, Simulator* sim) {
    // Next state latches
    PipelineLatch next_IF_ID = {0};
    PipelineLatch next_ID_EX = {0};
    PipelineLatch next_EX_MEM = {0};
    PipelineLatch next_MEM_WB = {0};
    
    bool branch_taken = false;
    uint32_t branch_target = 0;
    bool mem_completed = true;
    bool inst_completed = false;
    
    // Track what will complete WB this cycle (current MEM_WB before execution)
    // This becomes WB_completed for the NEXT cycle's trace
    PipelineLatch completing_this_cycle = core->MEM_WB;
    
    // --- Execute stages (in reverse order for dependencies) ---
    
    // WB: commits this cycle (updates registers)
    inst_completed = do_writeback(core);
    if (inst_completed) {
        core->instruction_count++;
    }
    
    // MEM: may stall on cache miss
    if (!core->mem_stall) {
        mem_completed = do_mem(core, sim, &next_MEM_WB);
        if (!mem_completed) {
            core->mem_stall = true;
            core->mem_stall_cycles++;
        }
    } else {
        // Already stalled - try again
        mem_completed = do_mem(core, sim, &next_MEM_WB);
        if (mem_completed) {
            core->mem_stall = false;
        } else {
            core->mem_stall_cycles++;
        }
    }
    
    // If MEM is stalled, freeze pipeline (EX, ID, IF don't advance)
    if (core->mem_stall) {
        // WB still completed with the old MEM_WB, track it
        core->WB_completed = completing_this_cycle;
        return;  // Pipeline frozen
    }
    
    // EX: compute result
    do_execute(core, &next_EX_MEM);
    
    // ID: decode and resolve branches
    branch_taken = do_decode(core, sim, &next_ID_EX, &branch_target);
    
    // IF: fetch next instruction
    core->fetch_enabled = !core->decode_stall;
    
    if (core->fetch_enabled) {
        do_fetch(core, &next_IF_ID);
        // Advance PC (will be overwritten if branch taken)
        if (!core->halted && core->pc < IMEM_DEPTH) {
            core->pc = (core->pc + 1) & PC_MASK;
        }
    } else {
        // Keep IF_ID when decode stalled
        next_IF_ID = core->IF_ID;
    }
    
    // Handle branch taken (with delay slot)
    if (branch_taken && !core->decode_stall) {
        core->pc = branch_target;
    }
    
    // --- Update pipeline latches ---
    core->WB_completed = completing_this_cycle;  // Track what completed WB
    core->MEM_WB = next_MEM_WB;
    core->EX_MEM = next_EX_MEM;
    
    if (!core->decode_stall) {
        core->ID_EX = next_ID_EX;
        core->IF_ID = next_IF_ID;
    } else {
        // Decode stall: bubble in ID_EX, keep IF_ID unchanged
        core->ID_EX.valid = false;
    }
}
