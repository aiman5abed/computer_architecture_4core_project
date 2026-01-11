; =============================================================================
; Matrix Multiplication Test (Serial - Core 0 Only)
; =============================================================================
; Multiplies two 16x16 matrices: C = A * B
;
; Memory Layout:
;   A: 0x0000 - 0x00FF (256 words = 16x16)
;   B: 0x0100 - 0x01FF (256 words = 16x16)
;   C: 0x0200 - 0x02FF (256 words = 16x16)
;
; Register Usage:
;   R2 = i (row index, 0-15)
;   R3 = j (column index, 0-15)
;   R4 = k (inner loop index, 0-15)
;   R5 = sum (accumulator for C[i][j])
;   R6 = temp value
;   R7 = address calculation temp
;   R8 = 16 (loop bound)
;   R9 = base address of A (0x000)
;   R10 = base address of B (0x100)
;   R11 = base address of C (0x200)
;   R12 = A[i][k] address
;   R13 = B[k][j] address
;   R14 = temp
;
; Algorithm:
;   for i = 0 to 15:
;     for j = 0 to 15:
;       sum = 0
;       for k = 0 to 15:
;         sum += A[i][k] * B[k][j]
;       C[i][j] = sum
; =============================================================================

; Initialize constants
; R8 = 16 (loop bound)
00810001
00000010
; R9 = 0x000 (base A)
00910001
00000000
; R10 = 0x100 (base B)
00A10001
00000100
; R11 = 0x200 (base C)
00B10001
00000200

; R2 = 0 (i = 0)
00200000

; outer_loop (i loop): PC = 5
; R3 = 0 (j = 0)
00300000

; middle_loop (j loop): PC = 6
; R5 = 0 (sum = 0)
00500000
; R4 = 0 (k = 0)
00400000

; inner_loop (k loop): PC = 8
; Calculate A[i][k] address: R9 + i*16 + k = R9 + R2*16 + R4
; R7 = R2 << 4 (i * 16)
06721001
00000004
; R7 = R7 + R4 (i*16 + k)
00774000
; R12 = R9 + R7 (base A + offset)
00C97000
; Load A[i][k] into R6
10C60000

; Calculate B[k][j] address: R10 + k*16 + j = R10 + R4*16 + R3
; R7 = R4 << 4 (k * 16)
06741001
00000004
; R7 = R7 + R3 (k*16 + j)
00773000
; R13 = R10 + R7 (base B + offset)
00DA7000
; Load B[k][j] into R14
10DE0000

; R14 = A[i][k] * B[k][j]
05E6E000
; R5 = R5 + R14 (sum += A*B)
0055E000

; k++
00441001
00000001
; if k < 16, goto inner_loop (PC=8)
0B108408
00000008

; Store C[i][j]: calculate address R11 + i*16 + j
; R7 = R2 << 4 (i * 16)
06721001
00000004
; R7 = R7 + R3 (i*16 + j)
00773000
; R7 = R11 + R7 (base C + offset)
007B7000
; Store sum to C[i][j]
11750000

; j++
00331001
00000001
; if j < 16, goto middle_loop (PC=6)
0B108306
00000006

; i++
00221001
00000001
; if i < 16, goto outer_loop (PC=5)
0B108205
00000005

; Done
15000000
