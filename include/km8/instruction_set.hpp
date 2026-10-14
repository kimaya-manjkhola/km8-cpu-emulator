// instruction_set.hpp - a table describing every KM-8 instruction: its name,
// opcode and the kind of each operand. The assembler uses it to turn text
// into bytes; the table is the one place where names map to opcodes.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace km8 {

// What kind of byte an operand is. All three are one byte in machine code;
// the difference matters to the assembler (what text is allowed) and to the
// trace output (how the operand is printed).
enum class OperandType {
    Register,  // A, B, C or D, encoded as 0-3
    Value,     // an immediate number 0-255, e.g. the 42 in "LDI A, 42"
    Address,   // a memory address 0-255, or a label that stands for one
};

struct InstructionInfo {
    std::uint8_t opcode;
    std::string name;
    std::vector<OperandType> operands;  // in the order they appear in memory
};

// The full instruction set, in opcode order.
const std::vector<InstructionInfo>& instructionSet();

// Look up an instruction. Both return nullptr if there is no match.
// `name` must be upper case, e.g. "ADD".
const InstructionInfo* findInstructionByName(const std::string& name);
const InstructionInfo* findInstructionByOpcode(std::uint8_t opcode);

}  // namespace km8
