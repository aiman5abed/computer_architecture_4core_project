; =============================================================================
; Test Program 1: Synchronized Counter
; =============================================================================
; Four cores increment a shared counter in main memory one after another.
; Each core waits for its turn, increments the counter, then signals next core.
; 
; Memory layout:
;   Address 0x1000: Shared counter (starts at 0)
;   Address 0x1001: Turn indicator (0-3 indicates which core's turn)
;
; Each core:
;   1. Waits until turn == core_id
;   2. Loads counter, increments it, stores back
;   3. Sets turn = (core_id + 1) % 4
;   4. Repeats until counter reaches target value
;
; This tests:
;   - MESI coherency under contention
;   - Cache invalidation
;   - Write-back behavior
; =============================================================================

; ----- Core 0 IMEM (imem0.txt) -----
; Register usage:
;   R2 = core_id (0)
;   R3 = target count (128 = each core does 32 increments)
;   R4 = counter address (0x1000)
;   R5 = turn address (0x1001)
;   R6 = temp (loaded turn value)
;   R7 = temp (loaded counter value)
;   R8 = next turn value
;   R9 = number 4 (for modulo)

; Initialize registers
; add R2, R0, R0       ; R2 = 0 (core_id)
; Note: Use immediate in R1 for constants

; Encoding: opcode(8) rd(4) rs(4) rt(4) imm(12)
; ADD  = 0x00
; SUB  = 0x01
; BEQ  = 0x09
; BNE  = 0x0A
; BLT  = 0x0B
; LW   = 0x10
; SW   = 0x11
; HALT = 0x14

; Line 0: R2 = 0 (core_id) - add R2, R0, R0
00200000
; Line 1: R3 = 128 (target) - add R3, R0, R1 with imm=128
00310001
00000080
; Line 2: R4 = 0x1000 (counter addr) - add R4, R0, R1 with imm=0x1000
00410001
00001000
; Line 3: R5 = 0x1001 (turn addr) - add R5, R0, R1 with imm=0x1001
00510001
00001001
; Line 4: R9 = 4 - add R9, R0, R1 with imm=4
00910001
00000004

; wait_loop: (PC = 5)
; Line 5: R6 = MEM[R5] (load turn)
10650000
; Line 6: bne R6, R2, wait_loop (if turn != core_id, keep waiting)
0A000625
00000005

; Got our turn - increment counter
; Line 7: R7 = MEM[R4] (load counter)
10740000
; Line 8: R7 = R7 + 1
00771001
00000001
; Line 9: MEM[R4] = R7 (store counter)
11740000

; Update turn to next core
; Line 10: R8 = R2 + 1 (next core)
00821001
00000001
; Line 11: Check if R8 == 4, wrap to 0
09089000
0000000E
; Line 12: R8 = 0 (wrapped)
00800000
; Line 13: jump to store_turn
0F00D000
00000010
; Line 14: (target of R8==4 check, already R8=next)
; Line 15: MEM[R5] = R8 (store turn)
11850000

; Check if counter reached target
; Line 16: blt R7, R3, wait_loop
0B070305
00000005

; Done
; Line 17: HALT
14000000
