# KM-8 Specification

This document is the source of truth for the KM-8 design.
Do not change it without asking first.

## Machine

- 8-bit CPU. All values 0-255; arithmetic wraps (255 + 1 = 0).
- 4 general registers: A, B, C, D (register codes 0, 1, 2, 3).
- PC (program counter, 8-bit), starts at 0. Programs load at address 0.
- SP (stack pointer, 8-bit), starts at 0. PUSH decrements SP first, then writes, so the first push goes to address 255. The stack grows downwards.
- Flags: Z (zero) and C (carry/borrow).
- Memory: 256 bytes.
- Each instruction = 1 opcode byte, then 0, 1 or 2 operand bytes.
- An unknown opcode or an invalid register code stops the CPU with a clear error.
- Running has a maximum cycle limit so infinite loops cannot hang.

## Instructions

| Opcode | Name  | Operands  | Meaning                                              | Flags                              |
|--------|-------|-----------|------------------------------------------------------|------------------------------------|
| 0x00   | HALT  |           | stop                                                 |                                    |
| 0x01   | NOP   |           | do nothing                                           |                                    |
| 0x02   | LDI   | r, value  | r = value                                            |                                    |
| 0x03   | LOAD  | r, addr   | r = memory[addr]                                     |                                    |
| 0x04   | STORE | r, addr   | memory[addr] = r                                     |                                    |
| 0x05   | MOV   | r1, r2    | r1 = r2                                              |                                    |
| 0x10   | ADD   | r1, r2    | r1 = r1 + r2                                         | Z if result 0; C if result > 255   |
| 0x11   | SUB   | r1, r2    | r1 = r1 - r2                                         | Z if result 0; C if r2 > r1 (borrow) |
| 0x12   | INC   | r         | r = r + 1                                            | Z only                             |
| 0x13   | DEC   | r         | r = r - 1                                            | Z only                             |
| 0x20   | AND   | r1, r2    |                                                      | Z; C cleared                       |
| 0x21   | OR    | r1, r2    |                                                      | Z; C cleared                       |
| 0x22   | XOR   | r1, r2    |                                                      | Z; C cleared                       |
| 0x30   | CMP   | r1, r2    | like SUB but only sets flags; r1 unchanged           |                                    |
| 0x40   | JMP   | addr      | PC = addr                                            |                                    |
| 0x41   | JZ    | addr      | jump if Z set                                        |                                    |
| 0x42   | JNZ   | addr      | jump if Z clear                                      |                                    |
| 0x43   | JC    | addr      | jump if C set                                        |                                    |
| 0x50   | PUSH  | r         | SP = SP - 1; memory[SP] = r                          |                                    |
| 0x51   | POP   | r         | r = memory[SP]; SP = SP + 1                          |                                    |
| 0x52   | CALL  | addr      | push the address of the next instruction, then PC = addr |                                |
| 0x53   | RET   |           | pop an address into PC                               |                                    |

## Known limitations

- No interrupts, no input/output devices, no pipeline or cache.
- The stack can grow into program memory; this is not detected.
