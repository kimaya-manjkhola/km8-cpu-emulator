; add.asm - add two numbers.
;
; Loads 25 into A and 17 into B, then adds them.
; Result: A = 42.

        LDI A, 25       ; first number
        LDI B, 17       ; second number
        ADD A, B        ; A = A + B = 42 (no carry, so C stays 0)
        HALT
