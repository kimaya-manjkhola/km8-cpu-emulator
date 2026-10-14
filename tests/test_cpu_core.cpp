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
// LDR and STR (register-indirect addressing)
// ---------------------------------------------------------------------------

TEST_CASE("LDR loads from the address held in a register", "[cpu][ldr]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 0x5A,
        op::STORE, reg::A, 0x90,  // put a value at 0x90
        op::LDI, reg::B, 0x90,    // B points at it
        op::LDR, reg::C, reg::B,  // C = memory[B]
        op::HALT,
    });
    REQUIRE(cpu.registerValue(reg::C) == 0x5A);
    REQUIRE(cpu.registerValue(reg::B) == 0x90);  // the pointer is unchanged
}

TEST_CASE("LDR from address 0 reads the first program byte", "[cpu][ldr][edge]") {
    Cpu cpu = runProgram({
        op::LDI, reg::B, 0,       // address 0 holds this LDI opcode
        op::LDR, reg::A, reg::B,
        op::HALT,
    });
    REQUIRE(cpu.registerValue(reg::A) == op::LDI);
}

TEST_CASE("LDR from address 255 reads the last byte of memory", "[cpu][ldr][edge]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 0xC3,
        op::STORE, reg::A, 255,
        op::LDI, reg::B, 255,
        op::LDR, reg::D, reg::B,
        op::HALT,
    });
    REQUIRE(cpu.registerValue(reg::D) == 0xC3);
}

TEST_CASE("LDR can use the same register as pointer and destination", "[cpu][ldr][edge]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 0x11,
        op::STORE, reg::A, 0x80,
        op::LDI, reg::A, 0x80,    // A = pointer
        op::LDR, reg::A, reg::A,  // A = memory[A]: the pointer is read first
        op::HALT,
    });
    REQUIRE(cpu.registerValue(reg::A) == 0x11);
}

TEST_CASE("STR stores to the address held in a register", "[cpu][str]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 0x5A,
        op::LDI, reg::B, 0x80,
        op::STR, reg::A, reg::B,  // memory[B] = A
        op::HALT,
    });
    REQUIRE(cpu.memory()[0x80] == 0x5A);
    REQUIRE(cpu.registerValue(reg::A) == 0x5A);  // the value is unchanged
    REQUIRE(cpu.registerValue(reg::B) == 0x80);  // the pointer is unchanged
}

TEST_CASE("STR to address 0 overwrites the first program byte", "[cpu][str][edge]") {
    // The program has already run past address 0, so overwriting it is safe
    // here; it shows that STR can reach the bottom of memory.
    Cpu cpu = runProgram({
        op::LDI, reg::A, 0x77,
        op::LDI, reg::B, 0,
        op::STR, reg::A, reg::B,
        op::HALT,
    });
    REQUIRE(cpu.memory()[0] == 0x77);
}

TEST_CASE("STR to address 255 writes the last byte of memory", "[cpu][str][edge]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 0xEE,
        op::LDI, reg::B, 255,
        op::STR, reg::A, reg::B,
        op::HALT,
    });
    REQUIRE(cpu.memory()[255] == 0xEE);
}

TEST_CASE("STR then LDR through a pointer round-trips a value", "[cpu][ldr][str]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 200,
        op::LDI, reg::B, 0xA0,
        op::STR, reg::A, reg::B,
        op::LDR, reg::C, reg::B,
        op::HALT,
    });
    REQUIRE(cpu.registerValue(reg::C) == 200);
}

namespace {

// Run `flagSetup`, then "LDI B, 0x80" and "<opcode> A, B", then HALT.
// A is 0 and memory[0x80] is 0, so if LDR or STR wrongly updated the flags
// from that value, Z would change.
Cpu runAfterFlagSetup(std::vector<std::uint8_t> flagSetup, std::uint8_t opcode) {
    flagSetup.insert(flagSetup.end(), {op::LDI, reg::B, 0x80, opcode, reg::A, reg::B, op::HALT});
    return runProgram(flagSetup);
}

// 255 + 1 = 0 with a carry: Z and C both set.
const std::vector<std::uint8_t> kSetBothFlags = {
    op::LDI, reg::D, 255, op::LDI, reg::C, 1, op::ADD, reg::D, reg::C,
};
// 1 + 1 = 2: Z and C both clear.
const std::vector<std::uint8_t> kClearBothFlags = {
    op::LDI, reg::D, 1, op::ADD, reg::D, reg::D,
};

}  // namespace

TEST_CASE("LDR and STR do not change the flags", "[cpu][ldr][str][flags]") {
    // Checked with both flags set and with both clear, so a wrong "set"
    // and a wrong "clear" would both be caught.
    Cpu ldrSet = runAfterFlagSetup(kSetBothFlags, op::LDR);
    REQUIRE(ldrSet.zeroFlag());
    REQUIRE(ldrSet.carryFlag());

    Cpu ldrClear = runAfterFlagSetup(kClearBothFlags, op::LDR);
    REQUIRE_FALSE(ldrClear.zeroFlag());
    REQUIRE_FALSE(ldrClear.carryFlag());

    Cpu strSet = runAfterFlagSetup(kSetBothFlags, op::STR);
    REQUIRE(strSet.zeroFlag());
    REQUIRE(strSet.carryFlag());

    Cpu strClear = runAfterFlagSetup(kClearBothFlags, op::STR);
    REQUIRE_FALSE(strClear.zeroFlag());
    REQUIRE_FALSE(strClear.carryFlag());
}

TEST_CASE("LDR and STR reject invalid register codes", "[cpu][ldr][str][errors]") {
    SECTION("LDR with a bad pointer register") {
        Cpu cpu;
        cpu.loadProgram({op::LDR, reg::A, 4});
        REQUIRE_THROWS_WITH(cpu.step(), "invalid register code 4 at address 0x00");
        REQUIRE(cpu.isHalted());
    }
    SECTION("STR with a bad value register") {
        Cpu cpu;
        cpu.loadProgram({op::STR, 9, reg::B});
        REQUIRE_THROWS_WITH(cpu.step(), "invalid register code 9 at address 0x00");
        REQUIRE(cpu.isHalted());
    }
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
    // 0x08 sits between STR (0x07) and ADD (0x10) and is not an instruction.
    Cpu cpu;
    cpu.loadProgram({0x08});
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
