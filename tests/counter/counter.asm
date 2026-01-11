ADD R2, R0, R0, 4       % R2 = Address of Turn (4)
    ADD R4, R0, R0, 0       % R4 = Address of Counter (0)
    ADD R7, R0, R0, 128     % R7 = 128 (Loop Limit)
    ADD R8, R0, R0, 1       % R8 = 1 (Constant)
    ADD R6, R0, R0, 0       % R6 = 0 (Loop Counter i)
    ADD R10, R0, R0, 4      % R10 = 4 (Max Cores / Turn Wrap)

    % Get Core ID (Pseudo-instruction: In the simulator this is hardcoded per file)
    % ADD R1, R0, R0, [0/1/2/3] % R1 = MyCoreID

LOOP_START:
SPIN_LOCK:
    LW  R3, R2, R0, 0       % Load Turn (MEM[4])
    BNE R3, R1, SPIN_LOCK   % If Turn != MyID, keep spinning

    LW  R5, R4, R0, 0       % Load Counter (MEM[0])
    ADD R5, R5, R8, 0       % Counter++
    SW  R5, R4, R0, 0       % Store Counter (MEM[0])

    ADD R9, R1, R8, 0       % Next = MyID + 1
    BEQ R9, R10, RESET_ID   % If Next == 4, Reset to 0
    BEQ R0, R0, UPDATE_TURN % Jump to Update
RESET_ID:
    ADD R9, R0, R0, 0       % Next = 0
UPDATE_TURN:
    SW  R9, R2, R0, 0       % Store Turn (MEM[4])

    ADD R6, R6, R8, 0       % i++
    BNE R6, R7, LOOP_START  % If i != 128, Loop

    HALT