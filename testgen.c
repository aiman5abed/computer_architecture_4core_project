#include <stdio.h>
#include <stdint.h>

// Opcodes
#define OP_ADD  0x00
#define OP_SUB  0x01
#define OP_MUL  0x05
#define OP_BEQ  0x09
#define OP_BNE  0x0A  // Using BNE (10) instead of BLT (11)
#define OP_LW   0x10
#define OP_SW   0x11
#define OP_HALT 0x14

// Registers
#define R0 0
#define R1 1 // i
#define R2 2 // j
#define R3 3 // k
#define R4 4 // Base A
#define R5 5 // Base B
#define R6 6 // Base C
#define R7 7 // Temp Addr / Sum
#define R8 8 // Val A
#define R9 9 // Val B
#define R10 10 // Constant 16 (Limit)
#define R11 11 // Constant 1
#define R12 12 // Temp calc

// Helper to format instruction
uint32_t enc(int op, int rd, int rs, int rt, int imm) {
    return ((op & 0xFF) << 24) | ((rd & 0xF) << 20) | ((rs & 0xF) << 16) | ((rt & 0xF) << 12) | (imm & 0xFFF);
}

int main() {
    // 1. Generate memin.txt (Matrix Inputs)
    FILE *fmem = fopen("memin.txt", "w");
    // Matrix A (256 words): Fill with 1
    for (int i = 0; i < 256; i++) fprintf(fmem, "%08X\n", 1);
    // Matrix B (256 words): Fill with 1
    for (int i = 0; i < 256; i++) fprintf(fmem, "%08X\n", 1);
    // Zero out the rest
    for (int i = 512; i < 1024; i++) fprintf(fmem, "00000000\n");
    fclose(fmem);
    printf("Generated memin.txt\n");

    // 2. Generate imem0.txt (Serial Matrix Mul - Fixed Branches)
    FILE *fimem = fopen("imem0.txt", "w");
    
    // 0: Init Base A (R4 = 0)
    fprintf(fimem, "%08X\n", enc(OP_ADD, R4, R0, R0, 0x000));
    // 1: Init Base B (R5 = 256)
    fprintf(fimem, "%08X\n", enc(OP_ADD, R5, R0, R0, 0x100));
    // 2: Init Base C (R6 = 512)
    fprintf(fimem, "%08X\n", enc(OP_ADD, R6, R0, R0, 0x200));
    // 3: Init Limit (R10 = 16)
    fprintf(fimem, "%08X\n", enc(OP_ADD, R10, R0, R0, 16));
    // 4: Init Step (R11 = 1)
    fprintf(fimem, "%08X\n", enc(OP_ADD, R11, R0, R0, 1));
    
    // 5: i = 0
    fprintf(fimem, "%08X\n", enc(OP_ADD, R1, R0, R0, 0)); 
    
    // 6: [Label LOOP_I_START] j = 0
    fprintf(fimem, "%08X\n", enc(OP_ADD, R2, R0, R0, 0)); 

    // 7: [Label LOOP_J_START] Sum = 0 
    fprintf(fimem, "%08X\n", enc(OP_ADD, R7, R0, R0, 0)); 
    // 8: k = 0
    fprintf(fimem, "%08X\n", enc(OP_ADD, R3, R0, R0, 0)); 

    // 9: [Label LOOP_K_START] Offset A = i * 16
    fprintf(fimem, "%08X\n", enc(OP_MUL, R12, R1, R10, 0)); 
    // 10: Offset A += k
    fprintf(fimem, "%08X\n", enc(OP_ADD, R12, R12, R3, 0)); 
    // 11: Load A (R8)
    fprintf(fimem, "%08X\n", enc(OP_LW,  R8, R4, R12, 0));

    // 12: Offset B = k * 16
    fprintf(fimem, "%08X\n", enc(OP_MUL, R12, R3, R10, 0)); 
    // 13: Offset B += j
    fprintf(fimem, "%08X\n", enc(OP_ADD, R12, R12, R2, 0)); 
    // 14: Load B (R9)
    fprintf(fimem, "%08X\n", enc(OP_LW,  R9, R5, R12, 0));

    // 15: Mult and Add
    fprintf(fimem, "%08X\n", enc(OP_MUL, R12, R8, R9, 0)); 
    fprintf(fimem, "%08X\n", enc(OP_ADD, R7, R7, R12, 0));

    // 16: k++
    fprintf(fimem, "%08X\n", enc(OP_ADD, R3, R3, R11, 0));
    // 17: Branch K if k != 16. Target: Line 9. 
    // Offset = 9 - (17 + 1) = -9 (0xFF7)
    fprintf(fimem, "%08X\n", enc(OP_BNE, 0, R3, R10, 0xFF7));   

    // 18: Offset C = i * 16
    fprintf(fimem, "%08X\n", enc(OP_MUL, R12, R1, R10, 0)); 
    // 19: Offset C += j
    fprintf(fimem, "%08X\n", enc(OP_ADD, R12, R12, R2, 0)); 
    // 20: Store C (Sum)
    fprintf(fimem, "%08X\n", enc(OP_SW,  R7, R6, R12, 0));

    // 21: j++
    fprintf(fimem, "%08X\n", enc(OP_ADD, R2, R2, R11, 0));
    // 22: Branch J if j != 16. Target: Line 7 (Sum=0). 
    // Offset = 7 - (22 + 1) = -16 (0xFF0)
    fprintf(fimem, "%08X\n", enc(OP_BNE, 0, R2, R10, 0xFF0)); 

    // 23: i++
    fprintf(fimem, "%08X\n", enc(OP_ADD, R1, R1, R11, 0));
    // 24: Branch I if i != 16. Target: Line 6 (j=0). 
    // Offset = 6 - (24 + 1) = -19 (0xFED)
    fprintf(fimem, "%08X\n", enc(OP_BNE, 0, R1, R10, 0xFED)); 

    // 25: Halt
    fprintf(fimem, "%08X\n", enc(OP_HALT, 0, 0, 0, 0));
    fclose(fimem);
    printf("Generated imem0.txt (Serial with BNE)\n");

    // 3. Generate empty files for other cores
    for(int i=1; i<4; i++) {
        char name[20];
        sprintf(name, "imem%d.txt", i);
        FILE *f = fopen(name, "w");
        fprintf(f, "%08X\n", enc(OP_HALT, 0, 0, 0, 0));
        fclose(f);
    }
    printf("Generated imem1-3.txt\n");
    return 0;
}
