#include <stdio.h>
#include <stdint.h>

// Opcodes
#define OP_ADD  0x00
#define OP_SUB  0x01
#define OP_MUL  0x05
#define OP_BEQ  0x09
#define OP_BNE  0x0A
#define OP_LW   0x10
#define OP_SW   0x11
#define OP_HALT 0x15

// Registers
#define R0 0
#define R1 1 // i (Row)
#define R2 2 // j (Col)
#define R3 3 // k (Dot Prod Iterator)
#define R4 4 // Base A
#define R5 5 // Base B
#define R6 6 // Base C
#define R7 7 // Sum
#define R8 8 // Val A
#define R9 9 // Val B
#define R10 10 // Constant 16 (Matrix Size)
#define R11 11 // Constant 1 (Step)
#define R12 12 // Temp
#define R13 13 // Loop Limit for i (End Row)

// Helper to format instruction
uint32_t enc(int op, int rd, int rs, int rt, int imm) {
    return ((op & 0xFF) << 24) | ((rd & 0xF) << 20) | ((rs & 0xF) << 16) | ((rt & 0xF) << 12) | (imm & 0xFFF);
}

int main() {
    // 1. Generate memin.txt (Inputs)
    FILE *fmem = fopen("memin.txt", "w");
    for (int i = 0; i < 512; i++) fprintf(fmem, "%08X\n", 1); // Fill A and B with 1s
    for (int i = 512; i < 1024; i++) fprintf(fmem, "00000000\n");
    fclose(fmem);
    printf("Generated memin.txt\n");

    // 2. Generate imemX.txt for ALL 4 CORES
    for (int core = 0; core < 4; core++) {
        char filename[20];
        sprintf(filename, "imem%d.txt", core);
        FILE *fimem = fopen(filename, "w");

        int start_row = core * 4;
        int end_row = start_row + 4;

        // Init Constants
        fprintf(fimem, "%08X\n", enc(OP_ADD, R4, R0, R0, 0x000)); // Base A
        fprintf(fimem, "%08X\n", enc(OP_ADD, R5, R0, R0, 0x100)); // Base B
        fprintf(fimem, "%08X\n", enc(OP_ADD, R6, R0, R0, 0x200)); // Base C
        fprintf(fimem, "%08X\n", enc(OP_ADD, R10, R0, R0, 16));   // Size 16
        fprintf(fimem, "%08X\n", enc(OP_ADD, R11, R0, R0, 1));    // Step 1

        // Init Loop Bounds for this Core
        fprintf(fimem, "%08X\n", enc(OP_ADD, R1, R0, R0, start_row)); // i = start_row
        fprintf(fimem, "%08X\n", enc(OP_ADD, R13, R0, R0, end_row));  // Limit = end_row

        // [Label LOOP_I] Check if i != Limit. If i == Limit, jump to Halt.
        // We do the check at the end (do-while style) or check before.
        // Let's stick to the previous structure:

        // 7: [Label LOOP_J_START] j = 0
        fprintf(fimem, "%08X\n", enc(OP_ADD, R2, R0, R0, 0));

        // 8: [Label LOOP_K_START] Sum = 0
        fprintf(fimem, "%08X\n", enc(OP_ADD, R7, R0, R0, 0));
        // 9: k = 0
        fprintf(fimem, "%08X\n", enc(OP_ADD, R3, R0, R0, 0));

        // 10: [Label LOOP_K_BODY] Offset A = i * 16
        fprintf(fimem, "%08X\n", enc(OP_MUL, R12, R1, R10, 0));
        // 11: Offset A += k
        fprintf(fimem, "%08X\n", enc(OP_ADD, R12, R12, R3, 0));
        // 12: Load A
        fprintf(fimem, "%08X\n", enc(OP_LW,  R8, R4, R12, 0));

        // 13: Offset B = k * 16
        fprintf(fimem, "%08X\n", enc(OP_MUL, R12, R3, R10, 0));
        // 14: Offset B += j
        fprintf(fimem, "%08X\n", enc(OP_ADD, R12, R12, R2, 0));
        // 15: Load B
        fprintf(fimem, "%08X\n", enc(OP_LW,  R9, R5, R12, 0));

        // 16: Mult and Add
        fprintf(fimem, "%08X\n", enc(OP_MUL, R12, R8, R9, 0));
        fprintf(fimem, "%08X\n", enc(OP_ADD, R7, R7, R12, 0));

        // 17: k++
        fprintf(fimem, "%08X\n", enc(OP_ADD, R3, R3, R11, 0));
        // 18: Branch K if k != 16. Target: Line 10. Offset = 10 - (18+1) = -9 (0xFF7)
        fprintf(fimem, "%08X\n", enc(OP_BNE, 0, R3, R10, 0xFF7));

        // 19: Offset C = i * 16
        fprintf(fimem, "%08X\n", enc(OP_MUL, R12, R1, R10, 0));
        // 20: Offset C += j
        fprintf(fimem, "%08X\n", enc(OP_ADD, R12, R12, R2, 0));
        // 21: Store C
        fprintf(fimem, "%08X\n", enc(OP_SW,  R7, R6, R12, 0));

        // 22: j++
        fprintf(fimem, "%08X\n", enc(OP_ADD, R2, R2, R11, 0));
        // 23: Branch J if j != 16. Target: Line 8 (Sum=0). Offset = 8 - (23+1) = -16 (0xFF0)
        fprintf(fimem, "%08X\n", enc(OP_BNE, 0, R2, R10, 0xFF0));

        // 24: i++
        fprintf(fimem, "%08X\n", enc(OP_ADD, R1, R1, R11, 0));
        // 25: Branch I if i != Limit (R13). Target: Line 7 (j=0). Offset = 7 - (25+1) = -19 (0xFED)
        fprintf(fimem, "%08X\n", enc(OP_BNE, 0, R1, R13, 0xFED));

        // 26: Halt
        fprintf(fimem, "%08X\n", enc(OP_HALT, 0, 0, 0, 0));
        fclose(fimem);
        printf("Generated imem%d.txt for Rows %d-%d\n", core, start_row, end_row - 1);
    }
    return 0;
}
