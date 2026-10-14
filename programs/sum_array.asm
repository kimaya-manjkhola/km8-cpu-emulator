; sum_array.asm - add up 5 numbers stored in memory, using a loop.
;
; Step 1 writes the array 10, 20, 30, 40, 50 to addresses 0x80-0x84
; (well clear of the program and of the stack at the top of memory).
; Step 2 walks through the array with a loop:
;   B is a pointer: it holds the address of the current element.
;   C is a counter: how many elements are left to add.
;   LDR D, B reads the element B points at ("register-indirect" addressing),
;   so the same instruction reads a different address each time round.
; Result: A = 150, and the total is also saved at address 0x85.

; ----- Step 1: put the array in memory -----
        LDI A, 10
        STORE A, 0x80
        LDI A, 20
        STORE A, 0x81
        LDI A, 30
        STORE A, 0x82
        LDI A, 40
        STORE A, 0x83
        LDI A, 50
        STORE A, 0x84

; ----- Step 2: add the elements in a loop -----
        LDI A, 0        ; A = running total
        LDI B, 0x80     ; B = pointer to the first element
        LDI C, 5        ; C = number of elements left

loop:   LDR D, B        ; D = memory[B], the current element
        ADD A, D        ; add it to the total
        INC B           ; move the pointer to the next element
        DEC C           ; one fewer element left; sets Z when C reaches 0
        JNZ loop        ; more elements? go round again

        STORE A, 0x85   ; save the total just after the array
        HALT
