    ADD R4, R0, R0, 0x000   % [00] R4 = Base Address A (0x000)
    ADD R5, R0, R0, 0x100   % [01] R5 = Base Address B (0x100)
    ADD R6, R0, R0, 0x200   % [02] R6 = Base Address C (0x200)
    ADD R10, R0, R0, 16     % [03] R10 = 16 (Matrix Size/Limit)
    ADD R11, R0, R0, 1      % [04] R11 = 1 (Step/Constant)

    % ------------------------------------------------------------------
    % Core Specific Initialization
    % The values for R14 (Start Row) and R13 (End Row) change per core.
    % Core 0: Start=0,  End=4
    % Core 1: Start=4,  End=8
    % Core 2: Start=8,  End=12
    % Core 3: Start=12, End=16
    % ------------------------------------------------------------------
    
    % Example for Core 0 (Adjust manually for other cores):
    ADD R14, R0, R0, 0      % [05] R14 = i = Start Row
    ADD R13, R0, R0, 4      % [06] R13 = End Row (Limit)

LOOP_I:
    ADD R2, R0, R0, 0       % [07] R2 = j = 0 (Col Iterator)

LOOP_J:
    ADD R7, R0, R0, 0       % [08] R7 = Sum = 0
    ADD R3, R0, R0, 0       % [09] R3 = k = 0 (Dot Product Iterator)

LOOP_K:
    MUL R12, R14, R10, 0    % [10] R12 = i * 16
    ADD R12, R12, R3, 0     % [11] R12 = (i * 16) + k
    LW  R8, R4, R12, 0      % [12] R8 = A[i][k] (Load from Base A)

    MUL R12, R3, R10, 0     % [13] R12 = k * 16
    ADD R12, R12, R2, 0     % [14] R12 = (k * 16) + j
    LW  R9, R5, R12, 0      % [15] R9 = B[k][j] (Load from Base B)

    MUL R12, R8, R9, 0      % [16] R12 = A[i][k] * B[k][j]
    ADD R7, R7, R12, 0      % [17] Sum += Product

    ADD R3, R3, R11, 0      % [18] k++
    ADD R15, R0, R0, 10     % [19] R15 = LOOP_K target (PC=10)
    BNE R15, R3, R10, 0     % [20] If k != 16, Loop K

    MUL R12, R14, R10, 0    % [21] R12 = i * 16
    ADD R12, R12, R2, 0     % [22] R12 = (i * 16) + j
    SW  R7, R6, R12, 0      % [23] C[i][j] = Sum (Store to Base C)

    ADD R2, R2, R11, 0      % [24] j++
    ADD R15, R0, R0, 8      % [25] R15 = LOOP_J target (PC=8)
    BNE R15, R2, R10, 0     % [26] If j != 16, Loop J

    ADD R14, R14, R11, 0    % [27] i++
    ADD R15, R0, R0, 7      % [28] R15 = LOOP_I target (PC=7)
    BNE R15, R14, R13, 0    % [29] If i != End Row, Loop I

    HALT                    % [30]
