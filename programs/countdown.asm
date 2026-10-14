; countdown.asm - count down from 10 to 0 using a loop.
;
; A is the counter. DEC sets the Z flag when A reaches 0, and JNZ
; ("jump if not zero") repeats the loop until then. Run with --trace
; to watch A go 10, 9, 8, ... 0.
; Result: A = 0, Z = 1.

        LDI A, 10       ; start the counter at 10
loop:   DEC A           ; count down by one; Z is set when A becomes 0
        JNZ loop        ; not zero yet? go round again
        HALT
