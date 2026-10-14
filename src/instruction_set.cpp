// instruction_set.cpp - the table of KM-8 instructions (see docs/SPEC.md).
#include "km8/instruction_set.hpp"

#include "km8/opcodes.hpp"

namespace km8 {

const std::vector<InstructionInfo>& instructionSet() {
    // Short aliases keep each table row on one line.
    constexpr OperandType R = OperandType::Register;
    constexpr OperandType V = OperandType::Value;
    constexpr OperandType M = OperandType::Address;  // M for "memory address"

    // A function-local static is built once, the first time it is needed,
    // and then shared by every caller.
    static const std::vector<InstructionInfo> table = {
        {op::HALT,  "HALT",  {}},
        {op::NOP,   "NOP",   {}},
        {op::LDI,   "LDI",   {R, V}},
        {op::LOAD,  "LOAD",  {R, M}},
        {op::STORE, "STORE", {R, M}},
        {op::MOV,   "MOV",   {R, R}},
        {op::ADD,   "ADD",   {R, R}},
        {op::SUB,   "SUB",   {R, R}},
        {op::INC,   "INC",   {R}},
        {op::DEC,   "DEC",   {R}},
        {op::AND,   "AND",   {R, R}},
        {op::OR,    "OR",    {R, R}},
        {op::XOR,   "XOR",   {R, R}},
        {op::CMP,   "CMP",   {R, R}},
        {op::JMP,   "JMP",   {M}},
        {op::JZ,    "JZ",    {M}},
        {op::JNZ,   "JNZ",   {M}},
        {op::JC,    "JC",    {M}},
        {op::PUSH,  "PUSH",  {R}},
        {op::POP,   "POP",   {R}},
        {op::CALL,  "CALL",  {M}},
        {op::RET,   "RET",   {}},
    };
    return table;
}

const InstructionInfo* findInstructionByName(const std::string& name) {
    // A linear search is fine: there are only 22 instructions.
    for (const InstructionInfo& info : instructionSet()) {
        if (info.name == name) {
            return &info;
        }
    }
    return nullptr;
}

const InstructionInfo* findInstructionByOpcode(std::uint8_t opcode) {
    for (const InstructionInfo& info : instructionSet()) {
        if (info.opcode == opcode) {
            return &info;
        }
    }
    return nullptr;
}

}  // namespace km8
