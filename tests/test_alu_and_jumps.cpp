// test_alu_and_jumps.cpp - tests for arithmetic (ADD, SUB, INC, DEC),
// logic (AND, OR, XOR), compare (CMP) and jumps (JMP, JZ, JNZ, JC).
#include <catch2/catch_test_macros.hpp>

#include "km8/cpu.hpp"
#include "km8/opcodes.hpp"
#include "test_helpers.hpp"

using namespace km8;
using km8::test::runProgram;

namespace {

// Two instructions that leave both Z and C set (255 + 1 = 0 with a carry).
// Used to check that later instructions clear or keep the flags correctly.
const std::vector<std::uint8_t> kSetBothFlags = {
    op::LDI, reg::D, 255,
    op::LDI, reg::C, 1,
    op::ADD, reg::D, reg::C,
};

// Join two byte vectors (used to put kSetBothFlags in front of a test).
std::vector<std::uint8_t> join(std::vector<std::uint8_t> first,
                               const std::vector<std::uint8_t>& second) {
    first.insert(first.end(), second.begin(), second.end());
    return first;
}

}  // namespace

// ---------------------------------------------------------------------------
// ADD
// ---------------------------------------------------------------------------

TEST_CASE("ADD adds two registers", "[alu][add]") {
    Cpu cpu = runProgram({op::LDI, reg::A, 2, op::LDI, reg::B, 3, op::ADD, reg::A, reg::B, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 5);
    REQUIRE(cpu.registerValue(reg::B) == 3);  // r2 is unchanged
    REQUIRE_FALSE(cpu.zeroFlag());
    REQUIRE_FALSE(cpu.carryFlag());
}

TEST_CASE("ADD 255 + 1 wraps to 0 with C and Z set", "[alu][add][edge]") {
    Cpu cpu = runProgram({op::LDI, reg::A, 255, op::LDI, reg::B, 1, op::ADD, reg::A, reg::B, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 0);
    REQUIRE(cpu.carryFlag());
    REQUIRE(cpu.zeroFlag());
}

TEST_CASE("ADD with a carry but a non-zero result sets only C", "[alu][add][edge]") {
    Cpu cpu = runProgram({op::LDI, reg::A, 200, op::LDI, reg::B, 100, op::ADD, reg::A, reg::B, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 44);  // 300 - 256
    REQUIRE(cpu.carryFlag());
    REQUIRE_FALSE(cpu.zeroFlag());
}

TEST_CASE("ADD 0 + 0 sets Z without C", "[alu][add][edge]") {
    Cpu cpu = runProgram({op::ADD, reg::A, reg::B, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 0);
    REQUIRE(cpu.zeroFlag());
    REQUIRE_FALSE(cpu.carryFlag());
}

TEST_CASE("ADD 255 + 0 does not carry", "[alu][add][edge]") {
    Cpu cpu = runProgram({op::LDI, reg::A, 255, op::ADD, reg::A, reg::B, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 255);
    REQUIRE_FALSE(cpu.carryFlag());
}

TEST_CASE("ADD of a register to itself doubles it", "[alu][add]") {
    Cpu cpu = runProgram({op::LDI, reg::C, 21, op::ADD, reg::C, reg::C, op::HALT});
    REQUIRE(cpu.registerValue(reg::C) == 42);
}

TEST_CASE("ADD clears flags left set by an earlier instruction", "[alu][add]") {
    Cpu cpu = runProgram(join(kSetBothFlags, {op::LDI, reg::A, 1, op::ADD, reg::A, reg::A, op::HALT}));
    REQUIRE(cpu.registerValue(reg::A) == 2);
    REQUIRE_FALSE(cpu.zeroFlag());
    REQUIRE_FALSE(cpu.carryFlag());
}

// ---------------------------------------------------------------------------
// SUB
// ---------------------------------------------------------------------------

TEST_CASE("SUB subtracts r2 from r1", "[alu][sub]") {
    Cpu cpu = runProgram({op::LDI, reg::A, 10, op::LDI, reg::B, 3, op::SUB, reg::A, reg::B, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 7);
    REQUIRE(cpu.registerValue(reg::B) == 3);
    REQUIRE_FALSE(cpu.zeroFlag());
    REQUIRE_FALSE(cpu.carryFlag());
}

TEST_CASE("SUB of equal values gives 0 with Z set and no borrow", "[alu][sub][edge]") {
    Cpu cpu = runProgram({op::LDI, reg::A, 5, op::LDI, reg::B, 5, op::SUB, reg::A, reg::B, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 0);
    REQUIRE(cpu.zeroFlag());
    REQUIRE_FALSE(cpu.carryFlag());
}

TEST_CASE("SUB 0 - 1 wraps to 255 with C set", "[alu][sub][edge]") {
    Cpu cpu = runProgram({op::LDI, reg::B, 1, op::SUB, reg::A, reg::B, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 255);
    REQUIRE(cpu.carryFlag());
    REQUIRE_FALSE(cpu.zeroFlag());
}

TEST_CASE("SUB of a bigger number borrows", "[alu][sub][edge]") {
    Cpu cpu = runProgram({op::LDI, reg::A, 3, op::LDI, reg::B, 10, op::SUB, reg::A, reg::B, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 249);  // 3 - 10 + 256
    REQUIRE(cpu.carryFlag());
}

TEST_CASE("SUB clears flags left set by an earlier instruction", "[alu][sub]") {
    Cpu cpu = runProgram(join(kSetBothFlags, {op::LDI, reg::A, 9, op::LDI, reg::B, 4, op::SUB, reg::A, reg::B, op::HALT}));
    REQUIRE(cpu.registerValue(reg::A) == 5);
    REQUIRE_FALSE(cpu.zeroFlag());
    REQUIRE_FALSE(cpu.carryFlag());
}

// ---------------------------------------------------------------------------
// INC and DEC
// ---------------------------------------------------------------------------

TEST_CASE("INC adds one", "[alu][inc]") {
    Cpu cpu = runProgram({op::LDI, reg::B, 41, op::INC, reg::B, op::HALT});
    REQUIRE(cpu.registerValue(reg::B) == 42);
    REQUIRE_FALSE(cpu.zeroFlag());
}

TEST_CASE("INC 255 wraps to 0 and sets Z but not C", "[alu][inc][edge]") {
    Cpu cpu = runProgram({op::LDI, reg::A, 255, op::INC, reg::A, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 0);
    REQUIRE(cpu.zeroFlag());
    REQUIRE_FALSE(cpu.carryFlag());  // INC changes Z only
}

TEST_CASE("INC leaves an existing C flag alone and clears Z", "[alu][inc][edge]") {
    Cpu cpu = runProgram(join(kSetBothFlags, {op::INC, reg::A, op::HALT}));
    REQUIRE(cpu.registerValue(reg::A) == 1);
    REQUIRE_FALSE(cpu.zeroFlag());
    REQUIRE(cpu.carryFlag());
}

TEST_CASE("DEC subtracts one", "[alu][dec]") {
    Cpu cpu = runProgram({op::LDI, reg::C, 10, op::DEC, reg::C, op::HALT});
    REQUIRE(cpu.registerValue(reg::C) == 9);
    REQUIRE_FALSE(cpu.zeroFlag());
}

TEST_CASE("DEC to 0 sets Z", "[alu][dec][edge]") {
    Cpu cpu = runProgram({op::LDI, reg::C, 1, op::DEC, reg::C, op::HALT});
    REQUIRE(cpu.registerValue(reg::C) == 0);
    REQUIRE(cpu.zeroFlag());
}

TEST_CASE("DEC 0 wraps to 255 without setting C", "[alu][dec][edge]") {
    Cpu cpu = runProgram({op::DEC, reg::A, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 255);
    REQUIRE_FALSE(cpu.zeroFlag());
    REQUIRE_FALSE(cpu.carryFlag());  // DEC changes Z only
}

TEST_CASE("DEC leaves an existing C flag alone", "[alu][dec][edge]") {
    Cpu cpu = runProgram(join(kSetBothFlags, {op::LDI, reg::A, 3, op::DEC, reg::A, op::HALT}));
    REQUIRE(cpu.registerValue(reg::A) == 2);
    REQUIRE(cpu.carryFlag());
}

// ---------------------------------------------------------------------------
// AND, OR, XOR
// ---------------------------------------------------------------------------

TEST_CASE("AND, OR and XOR combine bits", "[alu][logic]") {
    // 0b1100 and 0b1010 differ in two bits, so every result is different.
    const std::vector<std::uint8_t> setup = {op::LDI, reg::A, 0b1100, op::LDI, reg::B, 0b1010};

    SECTION("AND") {
        Cpu cpu = runProgram(join(setup, {op::AND, reg::A, reg::B, op::HALT}));
        REQUIRE(cpu.registerValue(reg::A) == 0b1000);
        REQUIRE_FALSE(cpu.zeroFlag());
    }
    SECTION("OR") {
        Cpu cpu = runProgram(join(setup, {op::OR, reg::A, reg::B, op::HALT}));
        REQUIRE(cpu.registerValue(reg::A) == 0b1110);
        REQUIRE_FALSE(cpu.zeroFlag());
    }
    SECTION("XOR") {
        Cpu cpu = runProgram(join(setup, {op::XOR, reg::A, reg::B, op::HALT}));
        REQUIRE(cpu.registerValue(reg::A) == 0b0110);
        REQUIRE_FALSE(cpu.zeroFlag());
    }
}

TEST_CASE("Logic results of 0 set Z", "[alu][logic][edge]") {
    SECTION("AND with no common bits") {
        Cpu cpu = runProgram({op::LDI, reg::A, 0xF0, op::LDI, reg::B, 0x0F, op::AND, reg::A, reg::B, op::HALT});
        REQUIRE(cpu.registerValue(reg::A) == 0);
        REQUIRE(cpu.zeroFlag());
    }
    SECTION("OR of two zeros") {
        Cpu cpu = runProgram({op::OR, reg::A, reg::B, op::HALT});
        REQUIRE(cpu.zeroFlag());
    }
    SECTION("XOR of a register with itself") {
        Cpu cpu = runProgram({op::LDI, reg::A, 0x5A, op::XOR, reg::A, reg::A, op::HALT});
        REQUIRE(cpu.registerValue(reg::A) == 0);
        REQUIRE(cpu.zeroFlag());
    }
}

TEST_CASE("Logic instructions clear C and update Z", "[alu][logic][edge]") {
    const std::vector<std::uint8_t> setup = join(kSetBothFlags, {op::LDI, reg::A, 0xFF, op::LDI, reg::B, 0x01});

    SECTION("AND") {
        Cpu cpu = runProgram(join(setup, {op::AND, reg::A, reg::B, op::HALT}));
        REQUIRE_FALSE(cpu.carryFlag());
        REQUIRE_FALSE(cpu.zeroFlag());
    }
    SECTION("OR") {
        Cpu cpu = runProgram(join(setup, {op::OR, reg::A, reg::B, op::HALT}));
        REQUIRE(cpu.registerValue(reg::A) == 0xFF);
        REQUIRE_FALSE(cpu.carryFlag());
    }
    SECTION("XOR") {
        Cpu cpu = runProgram(join(setup, {op::XOR, reg::A, reg::B, op::HALT}));
        REQUIRE(cpu.registerValue(reg::A) == 0xFE);
        REQUIRE_FALSE(cpu.carryFlag());
    }
}

// ---------------------------------------------------------------------------
// CMP
// ---------------------------------------------------------------------------

TEST_CASE("CMP of equal values sets Z and leaves the registers unchanged", "[alu][cmp]") {
    Cpu cpu = runProgram({op::LDI, reg::A, 7, op::LDI, reg::B, 7, op::CMP, reg::A, reg::B, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 7);
    REQUIRE(cpu.registerValue(reg::B) == 7);
    REQUIRE(cpu.zeroFlag());
    REQUIRE_FALSE(cpu.carryFlag());
}

TEST_CASE("CMP with r1 < r2 sets C (borrow) and leaves r1 unchanged", "[alu][cmp][edge]") {
    Cpu cpu = runProgram({op::LDI, reg::A, 3, op::LDI, reg::B, 10, op::CMP, reg::A, reg::B, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 3);
    REQUIRE(cpu.carryFlag());
    REQUIRE_FALSE(cpu.zeroFlag());
}

TEST_CASE("CMP with r1 > r2 clears both flags", "[alu][cmp]") {
    Cpu cpu = runProgram(join(kSetBothFlags, {op::LDI, reg::A, 10, op::LDI, reg::B, 3, op::CMP, reg::A, reg::B, op::HALT}));
    REQUIRE(cpu.registerValue(reg::A) == 10);
    REQUIRE_FALSE(cpu.zeroFlag());
    REQUIRE_FALSE(cpu.carryFlag());
}

TEST_CASE("CMP 0 with 1 behaves like SUB 0 - 1 but keeps r1", "[alu][cmp][edge]") {
    Cpu cpu = runProgram({op::LDI, reg::B, 1, op::CMP, reg::A, reg::B, op::HALT});
    REQUIRE(cpu.registerValue(reg::A) == 0);
    REQUIRE(cpu.carryFlag());
    REQUIRE_FALSE(cpu.zeroFlag());
}

// ---------------------------------------------------------------------------
// Data movement leaves flags alone
// ---------------------------------------------------------------------------

TEST_CASE("LDI, MOV, LOAD and STORE do not change the flags", "[cpu][flags]") {
    Cpu cpu = runProgram(join(kSetBothFlags, {
        op::LDI, reg::A, 5,
        op::MOV, reg::B, reg::A,
        op::STORE, reg::A, 0x80,
        op::LOAD, reg::C, 0x80,
        op::HALT,
    }));
    REQUIRE(cpu.zeroFlag());
    REQUIRE(cpu.carryFlag());
}

// ---------------------------------------------------------------------------
// Jumps
// ---------------------------------------------------------------------------

namespace {

constexpr std::uint8_t kTaken = 1;
constexpr std::uint8_t kNotTaken = 2;

// Build a program that runs `setup`, then a conditional jump. Register B ends
// up as kTaken if the jump was taken, or kNotTaken if it fell through:
//
//   <setup>
//   Jcc taken
//   LDI B, kNotTaken
//   HALT
//   taken: LDI B, kTaken
//   HALT
std::vector<std::uint8_t> branchProgram(std::vector<std::uint8_t> setup, std::uint8_t jumpOpcode) {
    auto takenAddress = static_cast<std::uint8_t>(setup.size() + 2 + 3 + 1);
    return join(std::move(setup), {
        jumpOpcode, takenAddress,
        op::LDI, reg::B, kNotTaken,
        op::HALT,
        op::LDI, reg::B, kTaken,
        op::HALT,
    });
}

// Flag set-up sequences
const std::vector<std::uint8_t> kZeroSet = {op::LDI, reg::A, 1, op::DEC, reg::A};     // 1 - 1 = 0
const std::vector<std::uint8_t> kZeroClear = {op::LDI, reg::A, 2, op::DEC, reg::A};   // 2 - 1 = 1
const std::vector<std::uint8_t> kCarrySet = {op::LDI, reg::C, 1, op::SUB, reg::A, reg::C};    // 0 - 1 borrows
const std::vector<std::uint8_t> kCarryClear = {op::LDI, reg::A, 5, op::SUB, reg::A, reg::C};  // 5 - 0

}  // namespace

TEST_CASE("JMP always jumps", "[jump][jmp]") {
    Cpu cpu = runProgram({
        op::JMP, 5,             // 0: skip the next instruction
        op::LDI, reg::A, 99,    // 2: skipped
        op::LDI, reg::B, 1,     // 5: lands here
        op::HALT,
    });
    REQUIRE(cpu.registerValue(reg::A) == 0);
    REQUIRE(cpu.registerValue(reg::B) == 1);
}

TEST_CASE("JMP sets PC to the target address", "[jump][jmp]") {
    Cpu cpu;
    cpu.loadProgram({op::JMP, 0x42});
    cpu.step();
    REQUIRE(cpu.pc() == 0x42);
}

TEST_CASE("JZ jumps only when Z is set", "[jump][jz]") {
    SECTION("taken") {
        REQUIRE(runProgram(branchProgram(kZeroSet, op::JZ)).registerValue(reg::B) == kTaken);
    }
    SECTION("not taken") {
        REQUIRE(runProgram(branchProgram(kZeroClear, op::JZ)).registerValue(reg::B) == kNotTaken);
    }
}

TEST_CASE("JNZ jumps only when Z is clear", "[jump][jnz]") {
    SECTION("taken") {
        REQUIRE(runProgram(branchProgram(kZeroClear, op::JNZ)).registerValue(reg::B) == kTaken);
    }
    SECTION("not taken") {
        REQUIRE(runProgram(branchProgram(kZeroSet, op::JNZ)).registerValue(reg::B) == kNotTaken);
    }
}

TEST_CASE("JC jumps only when C is set", "[jump][jc]") {
    SECTION("taken") {
        REQUIRE(runProgram(branchProgram(kCarrySet, op::JC)).registerValue(reg::B) == kTaken);
    }
    SECTION("not taken") {
        REQUIRE(runProgram(branchProgram(kCarryClear, op::JC)).registerValue(reg::B) == kNotTaken);
    }
}

TEST_CASE("A conditional jump that is not taken moves PC past its operand", "[jump]") {
    Cpu cpu;
    cpu.loadProgram({op::JZ, 0x80, op::HALT});  // Z is clear after reset
    cpu.step();
    REQUIRE(cpu.pc() == 2);
}

TEST_CASE("DEC and JNZ make a loop that runs the right number of times", "[jump][loop]") {
    Cpu cpu = runProgram({
        op::LDI, reg::A, 5,     // 0: loop counter
        op::INC, reg::B,        // 3: loop body - count iterations in B
        op::DEC, reg::A,        // 5
        op::JNZ, 3,             // 7: back to the body until A reaches 0
        op::HALT,               // 9
    });
    REQUIRE(cpu.registerValue(reg::A) == 0);
    REQUIRE(cpu.registerValue(reg::B) == 5);
    REQUIRE(cpu.zeroFlag());
}

TEST_CASE("ALU instructions reject invalid register codes", "[alu][errors]") {
    Cpu cpu;
    cpu.loadProgram({op::ADD, reg::A, 4});
    REQUIRE_THROWS_AS(cpu.step(), CpuError);
    REQUIRE(cpu.isHalted());
}
