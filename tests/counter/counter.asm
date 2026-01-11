    ADD R2, R0, R0, 4       % [00] R2 = Address of Turn (4)
    ADD R4, R0, R0, 0       % [01] R4 = Address of Counter (0)
    ADD R7, R0, R0, 128     % [02] R7 = 128 (Loop Limit)
    ADD R8, R0, R0, 1       % [03] R8 = 1 (Constant)
    ADD R6, R0, R0, 0       % [04] R6 = 0 (Loop Counter i)
    ADD R10, R0, R0, 4      % [05] R10 = 4 (Max Cores / Turn Wrap)

    % Get Core ID (Set per core before assembling)
    ADD R12, R0, R0, 0      % [06] R12 = MyCoreID (0/1/2/3)

LOOP_START:
SPIN_LOCK:
    LW  R3, R2, R0, 0       % [07] Load Turn (MEM[4])
    ADD R15, R0, R0, 7      % [08] R15 = SPIN_LOCK target (PC=7)
    BNE R15, R3, R12, 0     % [09] If Turn != MyID, keep spinning

    LW  R5, R4, R0, 0       % [10] Load Counter (MEM[0])
    ADD R5, R5, R8, 0       % [11] Counter++
    SW  R5, R4, R0, 0       % [12] Store Counter (MEM[0])

    ADD R9, R12, R8, 0      % [13] Next = MyID + 1
    ADD R15, R0, R0, 18     % [14] R15 = RESET_ID target (PC=18)
    BEQ R15, R9, R10, 0     % [15] If Next == 4, Reset to 0
    ADD R15, R0, R0, 19     % [16] R15 = UPDATE_TURN target (PC=19)
    BEQ R15, R0, R0, 0      % [17] Jump to Update
RESET_ID:
    ADD R9, R0, R0, 0       % [18] Next = 0
UPDATE_TURN:
    SW  R9, R2, R0, 0       % [19] Store Turn (MEM[4])

    ADD R6, R6, R8, 0       % [20] i++
    ADD R15, R0, R0, 7      % [21] R15 = LOOP_START target (PC=7)
    BNE R15, R6, R7, 0      % [22] If i != 128, Loop

    HALT                    % [23]
