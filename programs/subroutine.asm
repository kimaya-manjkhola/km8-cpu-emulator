; subroutine.asm - use CALL and RET.
;
; The subroutine "double" doubles A. The main program calls it twice.
; CALL pushes the address of the next instruction onto the stack and
; jumps; RET pops that address back into PC, so execution continues
; right after the CALL. The stack is empty again at the end (SP = 0).
; Result: A = 5 * 2 * 2 = 20.

main:   LDI A, 5
        CALL double     ; A = 10
        CALL double     ; A = 20
        HALT            ; stop here, before falling into the subroutine

; double: A = A + A
double: ADD A, A
        RET             ; return to the instruction after the CALL
