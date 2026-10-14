// test_assembler.cpp - tests for the instruction table and the assembler.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>

#include <set>
#include <string>

#include "km8/assembler.hpp"
#include "km8/cpu.hpp"
#include "km8/instruction_set.hpp"
#include "km8/opcodes.hpp"
#include "test_helpers.hpp"

using namespace km8;
using km8::test::runProgram;
using Bytes = std::vector<std::uint8_t>;

// ---------------------------------------------------------------------------
// Instruction table
// ---------------------------------------------------------------------------

TEST_CASE("The instruction table has 24 instructions with unique names and opcodes", "[isa]") {
    std::set<std::string> names;
    std::set<std::uint8_t> opcodes;
    for (const InstructionInfo& info : instructionSet()) {
        names.insert(info.name);
        opcodes.insert(info.opcode);
    }
    REQUIRE(instructionSet().size() == 24);
    REQUIRE(names.size() == 24);
    REQUIRE(opcodes.size() == 24);
}

TEST_CASE("The CPU accepts exactly the opcodes in the instruction table", "[isa][cpu]") {
    // Keeps the assembler's table and the CPU's decoder in agreement.
    // Operands of 0 are always valid (register A, value 0, address 0).
    for (int opcode = 0; opcode <= 255; ++opcode) {
        Cpu cpu;
        cpu.loadProgram({static_cast<std::uint8_t>(opcode), 0, 0});
        bool inTable = findInstructionByOpcode(static_cast<std::uint8_t>(opcode)) != nullptr;
        if (inTable) {
            REQUIRE_NOTHROW(cpu.step());
        } else {
            REQUIRE_THROWS_AS(cpu.step(), CpuError);
        }
    }
}

TEST_CASE("Looking up instructions by name and opcode", "[isa]") {
    REQUIRE(findInstructionByName("ADD")->opcode == op::ADD);
    REQUIRE(findInstructionByOpcode(op::CALL)->name == "CALL");
    REQUIRE(findInstructionByName("FOO") == nullptr);
    REQUIRE(findInstructionByOpcode(0xFF) == nullptr);
}

// ---------------------------------------------------------------------------
// Basic syntax
// ---------------------------------------------------------------------------

TEST_CASE("An empty source gives an empty program", "[asm]") {
    REQUIRE(assemble("").empty());
    REQUIRE(assemble("\n\n   \n").empty());
}

TEST_CASE("Instructions without operands", "[asm]") {
    REQUIRE(assemble("NOP\nRET\nHALT") == Bytes{op::NOP, op::RET, op::HALT});
}

TEST_CASE("Every instruction assembles to its opcode", "[asm]") {
    for (const InstructionInfo& info : instructionSet()) {
        // Build e.g. "LDI A, 5": registers become A, everything else 5.
        std::string line = info.name;
        for (std::size_t i = 0; i < info.operands.size(); ++i) {
            line += (i == 0) ? " " : ", ";
            line += (info.operands[i] == OperandType::Register) ? "A" : "5";
        }
        Bytes bytes = assemble(line);
        REQUIRE(bytes.size() == 1 + info.operands.size());
        REQUIRE(bytes[0] == info.opcode);
    }
}

TEST_CASE("Register names A-D become codes 0-3", "[asm][registers]") {
    REQUIRE(assemble("MOV A, B") == Bytes{op::MOV, 0, 1});
    REQUIRE(assemble("MOV C, D") == Bytes{op::MOV, 2, 3});
}

TEST_CASE("LDR and STR take two registers: value/destination, then pointer", "[asm][registers]") {
    REQUIRE(assemble("LDR A, B") == Bytes{op::LDR, reg::A, reg::B});
    REQUIRE(assemble("str c, d") == Bytes{op::STR, reg::C, reg::D});
}

TEST_CASE("Instruction and register names are not case-sensitive", "[asm]") {
    REQUIRE(assemble("ldi c, 1") == Bytes{op::LDI, reg::C, 1});
    REQUIRE(assemble("Add a, B") == Bytes{op::ADD, reg::A, reg::B});
}

TEST_CASE("Decimal and hex numbers", "[asm][numbers]") {
    REQUIRE(assemble("LDI A, 0") == Bytes{op::LDI, reg::A, 0});
    REQUIRE(assemble("LDI A, 255") == Bytes{op::LDI, reg::A, 255});
    REQUIRE(assemble("LDI A, 0xFF") == Bytes{op::LDI, reg::A, 255});
    REQUIRE(assemble("LDI A, 0x0a") == Bytes{op::LDI, reg::A, 10});
    REQUIRE(assemble("LDI A, 0X10") == Bytes{op::LDI, reg::A, 16});
    REQUIRE(assemble("LOAD B, 0x80") == Bytes{op::LOAD, reg::B, 0x80});
    REQUIRE(assemble("JMP 007") == Bytes{op::JMP, 7});  // leading zeros are fine
}

TEST_CASE("Comments, blank lines and extra whitespace are ignored", "[asm][comments]") {
    const std::string source =
        "; a whole-line comment\n"
        "\n"
        "   LDI   A ,  1   ; a trailing comment\n"
        "\tHALT\t\r\n";  // a tab and a Windows line ending
    REQUIRE(assemble(source) == Bytes{op::LDI, reg::A, 1, op::HALT});
}

// ---------------------------------------------------------------------------
// Labels
// ---------------------------------------------------------------------------

TEST_CASE("A label used before it is defined (forward reference)", "[asm][labels]") {
    const std::string source =
        "JMP end\n"     // 0
        "NOP\n"         // 2
        "end:\n"
        "HALT\n";       // 3
    REQUIRE(assemble(source) == Bytes{op::JMP, 3, op::NOP, op::HALT});
}

TEST_CASE("A label used after it is defined (backward reference)", "[asm][labels]") {
    const std::string source =
        "LDI A, 3\n"    // 0
        "loop:\n"
        "DEC A\n"       // 3
        "JNZ loop\n"    // 5
        "HALT\n";       // 7
    REQUIRE(assemble(source) == Bytes{op::LDI, reg::A, 3, op::DEC, reg::A, op::JNZ, 3, op::HALT});
}

TEST_CASE("A label can share a line with an instruction", "[asm][labels]") {
    REQUIRE(assemble("start: NOP\nJMP start") == Bytes{op::NOP, op::JMP, 0});
}

TEST_CASE("Labels work with CALL, LOAD and STORE", "[asm][labels]") {
    const std::string source =
        "CALL sub\n"      // 0
        "HALT\n"          // 2
        "sub: LOAD A, sub\n"  // 3
        "STORE A, sub\n"  // 6
        "RET\n";          // 9
    REQUIRE(assemble(source) == Bytes{op::CALL, 3, op::HALT, op::LOAD, 0, 3, op::STORE, 0, 3, op::RET});
}

TEST_CASE("Labels are case-sensitive and may contain digits and underscores", "[asm][labels]") {
    REQUIRE(assemble("Loop_2: JMP Loop_2") == Bytes{op::JMP, 0});
    REQUIRE_THROWS_AS(assemble("loop: JMP LOOP"), AssemblerError);
}

TEST_CASE("An assembled loop runs correctly on the CPU", "[asm][cpu]") {
    const std::string source =
        "        LDI A, 4\n"
        "        LDI B, 0\n"
        "loop:   INC B     ; count iterations\n"
        "        DEC A\n"
        "        JNZ loop\n"
        "        HALT\n";
    Cpu cpu = runProgram(assemble(source));
    REQUIRE(cpu.isHalted());
    REQUIRE(cpu.registerValue(reg::B) == 4);
}

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

namespace {

// Check that assembling `source` throws an AssemblerError with exactly `message`.
void requireError(const std::string& source, const std::string& message) {
    REQUIRE_THROWS_MATCHES(assemble(source), AssemblerError, Catch::Matchers::Message(message));
}

}  // namespace

TEST_CASE("Unknown instruction", "[asm][errors]") {
    requireError("NOP\nFOO A\n", "line 2: unknown instruction 'FOO'");
}

TEST_CASE("Wrong number of operands", "[asm][errors]") {
    requireError("LDI A", "line 1: LDI expects 2 operand(s), got 1");
    requireError("HALT A", "line 1: HALT expects 0 operand(s), got 1");
    requireError("INC A, B", "line 1: INC expects 1 operand(s), got 2");
    requireError("LDI A,", "line 1: missing operand");
    requireError("ADD A,,B", "line 1: ADD expects 2 operand(s), got 3");
}

TEST_CASE("Wrong kind of operand", "[asm][errors]") {
    requireError("LDI 5, 5", "line 1: expected a register (A, B, C or D), got '5'");
    requireError("INC E", "line 1: expected a register (A, B, C or D), got 'E'");
    requireError("LDI A, B", "line 1: expected a number, got 'B'");
    requireError("JMP -1", "line 1: expected an address or label, got '-1'");
    // LDR/STR take the address from a register, so a number is rejected.
    requireError("LDR A, 0x80", "line 1: expected a register (A, B, C or D), got '0x80'");
    requireError("STR A, loop", "line 1: expected a register (A, B, C or D), got 'loop'");
}

TEST_CASE("Bad numbers", "[asm][errors][numbers]") {
    requireError("LDI A, 256", "line 1: number '256' is out of range (0-255)");
    requireError("LDI A, 0x100", "line 1: number '0x100' is out of range (0-255)");
    requireError("LDI A, -1", "line 1: expected a number, got '-1'");
    requireError("LDI A, 0xZZ", "line 1: expected a number, got '0xZZ'");
    requireError("LDI A, 0x", "line 1: expected a number, got '0x'");
    requireError("LDI A, 12abc", "line 1: expected a number, got '12abc'");
    requireError("LDI A, 99999999999999999999", "line 1: number '99999999999999999999' is out of range (0-255)");
}

TEST_CASE("Undefined label", "[asm][errors][labels]") {
    requireError("NOP\nNOP\nJMP nowhere", "line 3: undefined label 'nowhere'");
}

TEST_CASE("Duplicate label", "[asm][errors][labels]") {
    requireError("here: NOP\nhere: NOP", "line 2: label 'here' is defined more than once");
}

TEST_CASE("Invalid label name", "[asm][errors][labels]") {
    requireError("1abc: NOP", "line 1: invalid label name '1abc'");
    requireError(": NOP", "line 1: invalid label name ''");
    requireError("my label: NOP", "line 1: invalid label name 'my label'");
}

TEST_CASE("Program larger than 256 bytes", "[asm][errors][size]") {
    // 128 two-byte "JMP 0" instructions fill memory exactly; one more byte is too many.
    std::string full;
    for (int i = 0; i < 128; ++i) {
        full += "JMP 0\n";
    }
    REQUIRE(assemble(full).size() == 256);
    requireError(full + "HALT\n", "line 129: program is larger than 256 bytes");
}

TEST_CASE("A label after a full 256-byte program cannot be used", "[asm][errors][size]") {
    std::string source = "JMP end\n";
    for (int i = 0; i < 127; ++i) {
        source += "JMP 0\n";
    }
    source += "end:\n";
    requireError(source, "line 1: label 'end' is outside memory");
}

TEST_CASE("AssemblerError reports the line number", "[asm][errors]") {
    try {
        assemble("NOP\n\n; comment\nBAD");
        FAIL("expected an AssemblerError");
    } catch (const AssemblerError& error) {
        REQUIRE(error.lineNumber() == 4);
    }
}
