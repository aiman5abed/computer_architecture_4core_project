ADD R4, R0, R0, 0x000   % R4 = Base Address A (0x000)
    ADD R5, R0, R0, 0x100   % R5 = Base Address B (0x100)
    ADD R6, R0, R0, 0x200   % R6 = Base Address C (0x200)
    ADD R10, R0, R0, 16     % R10 = 16 (Matrix Size/Limit)
    ADD R11, R0, R0, 1      % R11 = 1 (Step/Constant)

    % ------------------------------------------------------------------
    % Core Specific Initialization
    % The values for R1 (Start Row) and R13 (End Row) change per core.
    % Core 0: Start=0,  End=4
    % Core 1: Start=4,  End=8
    % Core 2: Start=8,  End=12
    % Core 3: Start=12, End=16
    % ------------------------------------------------------------------
    
    % Example for Core 0 (Adjust manually for other cores):
    ADD R1, R0, R0, 0       % R1 = i = Start Row
    ADD R13, R0, R0, 4      % R13 = End Row (Limit)

LOOP_I:
    ADD R2, R0, R0, 0       % R2 = j = 0 (Col Iterator)

LOOP_J:
    ADD R7, R0, R0, 0       % R7 = Sum = 0
    ADD R3, R0, R0, 0       % R3 = k = 0 (Dot Product Iterator)

LOOP_K:
    MUL R12, R1, R10, 0     % R12 = i * 16
    ADD R12, R12, R3, 0     % R12 = (i * 16) + k
    LW  R8, R4, R12, 0      % R8 = A[i][k] (Load from Base A)

    MUL R12, R3, R10, 0     % R12 = k * 16
    ADD R12, R12, R2, 0     % R12 = (k * 16) + j
    LW  R9, R5, R12, 0      % R9 = B[k][j] (Load from Base B)

    MUL R12, R8, R9, 0      % R12 = A[i][k] * B[k][j]
    ADD R7, R7, R12, 0      % Sum += Product

    ADD R3, R3, R11, 0      % k++
    BNE R3, R10, LOOP_K     % If k != 16, Loop K

    MUL R12, R1, R10, 0     % R12 = i * 16
    ADD R12, R12, R2, 0     % R12 = (i * 16) + j
    SW  R7, R6, R12, 0      % C[i][j] = Sum (Store to Base C)

    ADD R2, R2, R11, 0      % j++
    BNE R2, R10, LOOP_J     % If j != 16, Loop J

    ADD R1, R1, R11, 0      % i++
    BNE R1, R13, LOOP_I     % If i != End Row, Loop I

    HALT