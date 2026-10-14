; sum_array.asm - add up 5 numbers stored in memory.
;
; Step 1 writes the array 10, 20, 30, 40, 50 to addresses 0x80-0x84
; (well clear of the program and of the stack at the top of memory).
; Step 2 reads each number back and adds it to a total in A.
;
; KM-8's LOAD only takes a fixed address (there is no indexed addressing),
; so a loop cannot step through the array. Instead the sum is "unrolled":
; one LOAD and one ADD per element.
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

; ----- Step 2: add the elements -----
        LDI A, 0        ; A = running total
        LOAD B, 0x80    ; B = element 0
        ADD A, B
        LOAD B, 0x81    ; B = element 1
        ADD A, B
        LOAD B, 0x82    ; B = element 2
        ADD A, B
        LOAD B, 0x83    ; B = element 3
        ADD A, B
        LOAD B, 0x84    ; B = element 4
        ADD A, B

        STORE A, 0x85   ; save the total just after the array
        HALT
