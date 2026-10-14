; multiply.asm - multiply two numbers by repeated addition.
;
; KM-8 has no multiply instruction, so 6 x 7 is worked out as
; 6 + 6 + 6 + 6 + 6 + 6 + 6, using C as a counter of additions left.
; Like all KM-8 arithmetic, a product above 255 wraps around.
; Result: A = 42.

        LDI B, 6        ; first number (added each time round the loop)
        LDI C, 7        ; second number = how many times to add B
        LDI A, 0        ; A holds the running total
        LDI D, 0        ; D holds 0, to compare C against

        CMP C, D        ; is the counter already 0?
        JZ done         ; if so, the answer is 0: skip the loop entirely

loop:   ADD A, B        ; total = total + first number
        DEC C           ; one fewer addition left
        JNZ loop        ; repeat until the counter reaches 0

done:   HALT
