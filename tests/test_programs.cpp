// test_programs.cpp - assembles and runs every example in programs/ and
// checks its final answer. Also tests formatInstruction (the --trace text).
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include <string>

#include "km8/assembler.hpp"
#include "km8/cpu.hpp"
#include "km8/instruction_set.hpp"
#include "km8/opcodes.hpp"

using namespace km8;

namespace {

// KM8_PROGRAMS_DIR is set by CMakeLists.txt to the absolute path of
// programs/, so the tests work from any folder.
std::string readProgram(const std::string& fileName) {
    std::ifstream file(std::string(KM8_PROGRAMS_DIR) + "/" + fileName);
    REQUIRE(file.good());
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Assemble and run an example; fail the test unless it reaches HALT.
Cpu runExample(const std::string& fileName) {
    Cpu cpu;
    cpu.loadProgram(assemble(readProgram(fileName)));
    REQUIRE(cpu.run() == StopReason::Halted);
    return cpu;
}

}  // namespace

// ---------------------------------------------------------------------------
// Example programs
// ---------------------------------------------------------------------------

TEST_CASE("add.asm adds 25 and 17", "[programs]") {
    Cpu cpu = runExample("add.asm");
    REQUIRE(cpu.registerValue(reg::A) == 42);
    REQUIRE_FALSE(cpu.carryFlag());
}

TEST_CASE("countdown.asm counts down from 10 to 0", "[programs]") {
    Cpu cpu = runExample("countdown.asm");
    REQUIRE(cpu.registerValue(reg::A) == 0);
    REQUIRE(cpu.zeroFlag());
    // LDI, then 10 x (DEC + JNZ), then HALT
    REQUIRE(cpu.cycles() == 1 + 10 * 2 + 1);
}

TEST_CASE("multiply.asm multiplies 6 by 7", "[programs]") {
    Cpu cpu = runExample("multiply.asm");
    REQUIRE(cpu.registerValue(reg::A) == 42);
    REQUIRE(cpu.registerValue(reg::C) == 0);  // the counter ran out
}

TEST_CASE("sum_array.asm adds up 5 numbers in memory with a loop", "[programs]") {
    Cpu cpu = runExample("sum_array.asm");
    REQUIRE(cpu.registerValue(reg::A) == 150);
    REQUIRE(cpu.registerValue(reg::B) == 0x85);  // the pointer moved past all 5 elements
    REQUIRE(cpu.registerValue(reg::C) == 0);     // the counter ran out
    REQUIRE(cpu.memory()[0x80] == 10);
    REQUIRE(cpu.memory()[0x84] == 50);
    REQUIRE(cpu.memory()[0x85] == 150);  // the saved total
}

TEST_CASE("subroutine.asm doubles A twice with CALL and RET", "[programs]") {
    Cpu cpu = runExample("subroutine.asm");
    REQUIRE(cpu.registerValue(reg::A) == 20);
    REQUIRE(cpu.sp() == 0);  // every CALL was matched by a RET
}

// ---------------------------------------------------------------------------
// formatInstruction (used by --trace)
// ---------------------------------------------------------------------------

TEST_CASE("formatInstruction shows each operand type the way it is written", "[trace]") {
    Cpu cpu;
    cpu.loadProgram({
        op::LDI, reg::B, 10,      // 0: register and decimal value
        op::JNZ, 0x03,            // 3: address in hex
        op::ADD, reg::C, reg::D,  // 5: two registers
        op::HALT,                 // 8: no operands
        0xFF,                     // 9: not an instruction
        op::INC, 7,               // 10: invalid register code
    });
    const auto& memory = cpu.memory();
    REQUIRE(formatInstruction(memory, 0) == "LDI B, 10");
    REQUIRE(formatInstruction(memory, 3) == "JNZ 0x03");
    REQUIRE(formatInstruction(memory, 5) == "ADD C, D");
    REQUIRE(formatInstruction(memory, 8) == "HALT");
    REQUIRE(formatInstruction(memory, 9) == "??? (0xFF)");
    REQUIRE(formatInstruction(memory, 10) == "INC ?");
}

TEST_CASE("formatInstruction reverses the assembler", "[trace][asm]") {
    Cpu cpu;
    cpu.loadProgram(assemble("STORE D, 0x80\nCALL 0x2A\nLDR C, B\nSTR A, D"));
    REQUIRE(formatInstruction(cpu.memory(), 0) == "STORE D, 0x80");
    REQUIRE(formatInstruction(cpu.memory(), 3) == "CALL 0x2A");
    REQUIRE(formatInstruction(cpu.memory(), 5) == "LDR C, B");
    REQUIRE(formatInstruction(cpu.memory(), 8) == "STR A, D");
}
