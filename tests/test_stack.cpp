// test_stack.cpp - tests for the stack (PUSH, POP) and subroutines (CALL, RET).
#include <catch2/catch_test_macros.hpp>

#include "km8/cpu.hpp"
#include "km8/opcodes.hpp"
#include "test_helpers.hpp"

using namespace km8;
using km8::test::runProgram;

// ---------------------------------------------------------------------------
// PUSH and POP
// ---------------------------------------------------------------------------

TEST_CASE("The first PUSH writes to address 255", "[stack][push]") {
    Cpu cpu = runProgram({op::LDI, reg::A, 0x77, op::PUSH, reg::A, op::HALT});
    REQUIRE(cpu.sp() == 255);
    REQUIRE(cpu.memory()[255] == 0x77);
}

TEST_CASE("The stack grows downwards", "[stack][push]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 1,
        op::LDI, reg::B, 2,
        op::PUSH, reg::A,
        op::PUSH, reg::B,
        op::HALT,
    });
    REQUIRE(cpu.sp() == 254);
    REQUIRE(cpu.memory()[255] == 1);
    REQUIRE(cpu.memory()[254] == 2);
}

TEST_CASE("PUSH then POP restores the value and SP", "[stack][pop]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 123,
        op::PUSH, reg::A,
        op::LDI, reg::A, 0,   // overwrite A
        op::POP, reg::A,      // and get it back
        op::HALT,
    });
    REQUIRE(cpu.registerValue(reg::A) == 123);
    REQUIRE(cpu.sp() == 0);
}

TEST_CASE("POP returns values in last-in, first-out order", "[stack][pop]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 10,
        op::LDI, reg::B, 20,
        op::PUSH, reg::A,
        op::PUSH, reg::B,
        op::POP, reg::C,   // last pushed (20) comes out first
        op::POP, reg::D,
        op::HALT,
    });
    REQUIRE(cpu.registerValue(reg::C) == 20);
    REQUIRE(cpu.registerValue(reg::D) == 10);
    REQUIRE(cpu.sp() == 0);
}

TEST_CASE("PUSH and POP can move a value between registers", "[stack]") {
    Cpu cpu = runProgram({op::LDI, reg::A, 55, op::PUSH, reg::A, op::POP, reg::D, op::HALT});
    REQUIRE(cpu.registerValue(reg::D) == 55);
}

TEST_CASE("PUSH and POP do not change the flags", "[stack][flags]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 255,
        op::INC, reg::A,     // Z set
        op::PUSH, reg::A,
        op::POP, reg::B,
        op::HALT,
    });
    REQUIRE(cpu.zeroFlag());
}

TEST_CASE("POP on an empty stack wraps SP from 0 to 1 (not detected)", "[stack][edge]") {
    // The spec says stack misuse is not detected. With SP = 0, POP reads
    // memory[0] (here the POP opcode itself) and SP wraps upwards to 1.
    Cpu cpu = runProgram({op::POP, reg::A, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == op::POP);
    REQUIRE(cpu.sp() == 1);
}

TEST_CASE("PUSH and POP reject invalid register codes", "[stack][errors]") {
    Cpu cpu;
    cpu.loadProgram({op::PUSH, 9});
    REQUIRE_THROWS_AS(cpu.step(), CpuError);
}

// ---------------------------------------------------------------------------
// CALL and RET
// ---------------------------------------------------------------------------

TEST_CASE("CALL pushes the return address and jumps", "[stack][call]") {
    Cpu cpu;
    cpu.loadProgram({op::NOP, op::CALL, 0x10, op::HALT});
    cpu.step();  // NOP
    cpu.step();  // CALL 0x10 at address 1; the next instruction is at 3
    REQUIRE(cpu.pc() == 0x10);
    REQUIRE(cpu.sp() == 255);
    REQUIRE(cpu.memory()[255] == 3);
}

TEST_CASE("CALL then RET returns to the instruction after CALL", "[stack][call][ret]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 1,    // 0
        op::CALL, 9,           // 3: call the subroutine at 9
        op::INC, reg::A,       // 5: runs after the subroutine returns
        op::HALT,              // 7
        op::HALT,              // 8: never reached
        op::LDI, reg::B, 7,    // 9: subroutine
        op::RET,               // 12
    });
    REQUIRE(cpu.registerValue(reg::A) == 2);  // INC after the call ran
    REQUIRE(cpu.registerValue(reg::B) == 7);  // the subroutine ran
    REQUIRE(cpu.sp() == 0);                   // the stack is balanced again
    REQUIRE(cpu.pc() == 8);                   // stopped by the HALT at 7
}

TEST_CASE("RET pops the address into PC", "[stack][ret]") {
    Cpu cpu;
    cpu.loadProgram({op::LDI, reg::A, 0x40, op::PUSH, reg::A, op::RET});
    cpu.run(3);
    REQUIRE(cpu.pc() == 0x40);
    REQUIRE(cpu.sp() == 0);
}

TEST_CASE("Nested CALLs return in the right order", "[stack][call][ret]") {
    const std::vector<std::uint8_t> program = {
        op::CALL, 5,           // 0: main calls outer
        op::INC, reg::A,       // 2: after everything returns
        op::HALT,              // 4
        op::CALL, 8,           // 5: outer calls inner
        op::RET,               // 7: outer returns to main
        op::LDI, reg::B, 42,   // 8: inner
        op::RET,               // 11: inner returns to outer
    };

    SECTION("two return addresses are on the stack inside the inner call") {
        Cpu cpu;
        cpu.loadProgram(program);
        cpu.step();  // CALL 5
        cpu.step();  // CALL 8
        REQUIRE(cpu.pc() == 8);
        REQUIRE(cpu.sp() == 254);
        REQUIRE(cpu.memory()[255] == 2);  // return address into main
        REQUIRE(cpu.memory()[254] == 7);  // return address into outer
    }

    SECTION("the program finishes with everything unwound") {
        Cpu cpu = runProgram(program);
        REQUIRE(cpu.isHalted());
        REQUIRE(cpu.registerValue(reg::A) == 1);
        REQUIRE(cpu.registerValue(reg::B) == 42);
        REQUIRE(cpu.sp() == 0);
    }
}

TEST_CASE("A subroutine can save and restore a register with PUSH and POP", "[stack][call]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 5,    // 0
        op::CALL, 6,           // 3
        op::HALT,              // 5
        op::PUSH, reg::A,      // 6: save A
        op::LDI, reg::A, 99,   // 8: use A for something else
        op::MOV, reg::B, reg::A, // 11
        op::POP, reg::A,       // 14: restore A
        op::RET,               // 16
    });
    REQUIRE(cpu.registerValue(reg::A) == 5);
    REQUIRE(cpu.registerValue(reg::B) == 99);
    REQUIRE(cpu.sp() == 0);
}
