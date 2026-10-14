// test_cpu_core.cpp - tests for CPU state, loading, step/run, error handling
// and the data-movement instructions (HALT, NOP, LDI, LOAD, STORE, MOV).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "km8/cpu.hpp"
#include "km8/opcodes.hpp"
#include "test_helpers.hpp"

using namespace km8;
using km8::test::runProgram;
using Catch::Matchers::ContainsSubstring;

// ---------------------------------------------------------------------------
// Reset and loading
// ---------------------------------------------------------------------------

TEST_CASE("A new CPU starts in the power-on state", "[cpu]") {
    Cpu cpu;
    REQUIRE(cpu.pc() == 0);
    REQUIRE(cpu.sp() == 0);
    REQUIRE_FALSE(cpu.zeroFlag());
    REQUIRE_FALSE(cpu.carryFlag());
    REQUIRE_FALSE(cpu.isHalted());
    REQUIRE(cpu.cycles() == 0);
    for (std::uint8_t r = reg::A; r <= reg::D; ++r) {
        REQUIRE(cpu.registerValue(r) == 0);
    }
    for (std::uint8_t byte : cpu.memory()) {
        REQUIRE(byte == 0);
    }
}

TEST_CASE("loadProgram copies the program to address 0", "[cpu]") {
    Cpu cpu;
    cpu.loadProgram({0x11, 0x22, 0x33});
    REQUIRE(cpu.memory()[0] == 0x11);
    REQUIRE(cpu.memory()[1] == 0x22);
    REQUIRE(cpu.memory()[2] == 0x33);
    REQUIRE(cpu.memory()[3] == 0x00);
    REQUIRE(cpu.pc() == 0);
}

TEST_CASE("loadProgram accepts exactly 256 bytes but rejects 257", "[cpu]") {
    Cpu cpu;
    REQUIRE_NOTHROW(cpu.loadProgram(std::vector<std::uint8_t>(256, op::NOP)));
    REQUIRE_THROWS_AS(cpu.loadProgram(std::vector<std::uint8_t>(257, op::NOP)), CpuError);
}

TEST_CASE("reset clears registers, memory and the halted state", "[cpu]") {
    Cpu cpu = runProgram({op::LDI, reg::A, 7, op::STORE, reg::A, 0x80, op::HALT});
    REQUIRE(cpu.isHalted());

    cpu.reset();
    REQUIRE(cpu.registerValue(reg::A) == 0);
    REQUIRE(cpu.memory()[0x80] == 0);
    REQUIRE(cpu.memory()[0] == 0);
    REQUIRE(cpu.pc() == 0);
    REQUIRE(cpu.cycles() == 0);
    REQUIRE_FALSE(cpu.isHalted());
}

// ---------------------------------------------------------------------------
// HALT and NOP
// ---------------------------------------------------------------------------

TEST_CASE("HALT stops the CPU", "[cpu][halt]") {
    Cpu cpu;
    cpu.loadProgram({op::HALT});
    cpu.step();
    REQUIRE(cpu.isHalted());
    REQUIRE(cpu.pc() == 1);
    REQUIRE(cpu.cycles() == 1);

    SECTION("stepping a halted CPU does nothing") {
        cpu.step();
        REQUIRE(cpu.pc() == 1);
        REQUIRE(cpu.cycles() == 1);
    }
}

TEST_CASE("NOP only moves PC forward by one", "[cpu][nop]") {
    Cpu cpu;
    cpu.loadProgram({op::NOP, op::HALT});
    cpu.step();
    REQUIRE(cpu.pc() == 1);
    REQUIRE_FALSE(cpu.isHalted());
    REQUIRE(cpu.registerValue(reg::A) == 0);
    REQUIRE_FALSE(cpu.zeroFlag());
    REQUIRE_FALSE(cpu.carryFlag());
}

// ---------------------------------------------------------------------------
// LDI, LOAD, STORE, MOV
// ---------------------------------------------------------------------------

TEST_CASE("LDI loads an immediate value into a register", "[cpu][ldi]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 42,
        op::LDI, reg::B, 0,
        op::LDI, reg::C, 255,  // largest 8-bit value
        op::LDI, reg::D, 0x7F,
        op::HALT,
    });
    REQUIRE(cpu.registerValue(reg::A) == 42);
    REQUIRE(cpu.registerValue(reg::B) == 0);
    REQUIRE(cpu.registerValue(reg::C) == 255);
    REQUIRE(cpu.registerValue(reg::D) == 0x7F);
    REQUIRE(cpu.pc() == 13);  // 4 instructions x 3 bytes, plus HALT
}

TEST_CASE("LOAD reads a byte from memory into a register", "[cpu][load]") {
    // Programs load at address 0, so a data byte placed after HALT sits at
    // a known address (4) that LOAD can read.
    Cpu cpu = runProgram({
        op::LOAD, reg::B, 4,   // B = memory[4]
        op::HALT,
        0x2A,                  // address 4: data
    });
    REQUIRE(cpu.registerValue(reg::B) == 0x2A);
}

TEST_CASE("LOAD from an unused address gives 0", "[cpu][load]") {
    Cpu cpu = runProgram({op::LDI, reg::A, 99, op::LOAD, reg::A, 255, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 0);
}

TEST_CASE("STORE writes a register to memory", "[cpu][store]") {
    Cpu cpu = runProgram({
        op::LDI, reg::C, 0xAB,
        op::STORE, reg::C, 0x80,
        op::STORE, reg::C, 255,  // highest address
        op::HALT,
    });
    REQUIRE(cpu.memory()[0x80] == 0xAB);
    REQUIRE(cpu.memory()[255] == 0xAB);
    REQUIRE(cpu.registerValue(reg::C) == 0xAB);  // source is unchanged
}

TEST_CASE("STORE then LOAD round-trips a value", "[cpu][store][load]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 123,
        op::STORE, reg::A, 0x90,
        op::LOAD, reg::D, 0x90,
        op::HALT,
    });
    REQUIRE(cpu.registerValue(reg::D) == 123);
}

TEST_CASE("MOV copies one register into another", "[cpu][mov]") {
    Cpu cpu = runProgram({
        op::LDI, reg::B, 77,
        op::MOV, reg::A, reg::B,  // A = B
        op::HALT,
    });
    REQUIRE(cpu.registerValue(reg::A) == 77);
    REQUIRE(cpu.registerValue(reg::B) == 77);  // source is unchanged
}

TEST_CASE("MOV of a register to itself changes nothing", "[cpu][mov]") {
    Cpu cpu = runProgram({op::LDI, reg::D, 5, op::MOV, reg::D, reg::D, op::HALT});
    REQUIRE(cpu.registerValue(reg::D) == 5);
}

// ---------------------------------------------------------------------------
// Errors and the cycle limit
// ---------------------------------------------------------------------------

TEST_CASE("An unknown opcode stops the CPU with a clear error", "[cpu][errors]") {
    Cpu cpu;
    cpu.loadProgram({op::NOP, 0xFF});
    cpu.step();  // NOP is fine
    REQUIRE_THROWS_WITH(cpu.step(), "unknown opcode 0xFF at address 0x01");
    REQUIRE(cpu.isHalted());
}

TEST_CASE("Unused opcodes in the gaps are rejected too", "[cpu][errors]") {
    // 0x06 sits between MOV (0x05) and ADD (0x10) and is not an instruction.
    Cpu cpu;
    cpu.loadProgram({0x06});
    REQUIRE_THROWS_AS(cpu.step(), CpuError);
}

TEST_CASE("An invalid register code stops the CPU with a clear error", "[cpu][errors]") {
    SECTION("in the first operand") {
        Cpu cpu;
        cpu.loadProgram({op::LDI, 4, 10});
        REQUIRE_THROWS_WITH(cpu.step(), ContainsSubstring("invalid register code 4"));
        REQUIRE(cpu.isHalted());
    }
    SECTION("in the second operand") {
        Cpu cpu;
        cpu.loadProgram({op::MOV, reg::A, 200});
        REQUIRE_THROWS_WITH(cpu.step(), "invalid register code 200 at address 0x00");
        REQUIRE(cpu.isHalted());
    }
}

TEST_CASE("run returns Halted when the program reaches HALT", "[cpu][run]") {
    Cpu cpu;
    cpu.loadProgram({op::NOP, op::NOP, op::HALT});
    REQUIRE(cpu.run() == StopReason::Halted);
    REQUIRE(cpu.cycles() == 3);
}

TEST_CASE("run stops at the cycle limit so an endless program cannot hang", "[cpu][run]") {
    // 256 NOPs fill all of memory; PC wraps from 255 back to 0 and the
    // program never reaches a HALT.
    Cpu cpu;
    cpu.loadProgram(std::vector<std::uint8_t>(256, op::NOP));
    REQUIRE(cpu.run(1000) == StopReason::CycleLimit);
    REQUIRE(cpu.cycles() == 1000);
    REQUIRE_FALSE(cpu.isHalted());
    REQUIRE(cpu.pc() == 1000 % 256);  // shows PC wrapped around
}

TEST_CASE("Running off the end of a program hits empty memory, which is HALT", "[cpu][run]") {
    // Memory is zero-filled and 0x00 is HALT, so a program without HALT stops.
    Cpu cpu = runProgram({op::LDI, reg::A, 1});
    REQUIRE(cpu.isHalted());
    REQUIRE(cpu.registerValue(reg::A) == 1);
}
