// assembler.cpp - a simple two-pass assembler for KM-8.
//
// Pass 1 reads every line, records the address of each label and checks the
// instruction names and operand counts. Pass 2 turns each instruction into
// bytes. Two passes are needed so that a jump can use a label defined later
// in the file (a "forward reference"): by pass 2 every label's address is known.
#include "km8/assembler.hpp"

#include <cctype>
#include <cstddef>
#include <map>
#include <sstream>

#include "km8/cpu.hpp"
#include "km8/instruction_set.hpp"

namespace km8 {

AssemblerError::AssemblerError(int lineNumber, const std::string& message)
    : std::runtime_error("line " + std::to_string(lineNumber) + ": " + message),
      lineNumber_(lineNumber) {}

namespace {  // helpers used only in this file

// One instruction found in pass 1, waiting to be encoded in pass 2.
struct ParsedInstruction {
    int lineNumber;
    const InstructionInfo* info;
    std::vector<std::string> operands;  // operand text, e.g. {"A", "10"}
};

using LabelTable = std::map<std::string, int>;  // label name -> address

// ----- Small text helpers -----

std::string trim(const std::string& text) {
    const char* whitespace = " \t\r\n";  // \r handles Windows line endings
    std::size_t first = text.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return "";
    }
    std::size_t last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

std::string toUpper(std::string text) {
    for (char& ch : text) {
        // The cast avoids undefined behaviour for negative char values.
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return text;
}

std::string removeComment(const std::string& line) {
    return line.substr(0, line.find(';'));  // npos means "to the end"
}

bool startsWithDigit(const std::string& text) {
    return !text.empty() && std::isdigit(static_cast<unsigned char>(text[0]));
}

// A label must start with a letter or underscore, followed by letters,
// digits or underscores. Not starting with a digit is what lets the
// assembler tell a label ("loop") from a number ("10") in an operand.
bool isValidLabel(const std::string& name) {
    if (name.empty() || startsWithDigit(name)) {
        return false;
    }
    for (char ch : name) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
            return false;
        }
    }
    return true;
}

// Split "A, 10" into {"A", "10"}. An empty string gives no operands.
std::vector<std::string> splitOperands(const std::string& text) {
    std::vector<std::string> operands;
    if (trim(text).empty()) {
        return operands;
    }
    std::stringstream stream(text);
    std::string piece;
    while (std::getline(stream, piece, ',')) {
        operands.push_back(trim(piece));
    }
    if (text.back() == ',') {
        operands.push_back("");  // getline drops a trailing empty piece
    }
    return operands;
}

// ----- Operand parsing -----

// The value of one digit in base 16, or -1 if the character is not a digit.
int digitValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

// Parse a decimal ("42") or hex ("0x2A") number in the range 0-255.
std::uint8_t parseNumber(const std::string& text, int lineNumber) {
    bool isHex = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    int base = isHex ? 16 : 10;
    std::string digits = isHex ? text.substr(2) : text;

    if (digits.empty()) {
        throw AssemblerError(lineNumber, "expected a number, got '" + text + "'");
    }
    int value = 0;
    for (char ch : digits) {
        int digit = digitValue(ch);
        if (digit < 0 || digit >= base) {
            throw AssemblerError(lineNumber, "expected a number, got '" + text + "'");
        }
        value = value * base + digit;
        // Check inside the loop so a very long number cannot overflow `int`.
        if (value > 255) {
            throw AssemblerError(lineNumber, "number '" + text + "' is out of range (0-255)");
        }
    }
    return static_cast<std::uint8_t>(value);
}

std::uint8_t parseRegister(const std::string& text, int lineNumber) {
    const std::string name = toUpper(text);
    if (name == "A") return reg::A;
    if (name == "B") return reg::B;
    if (name == "C") return reg::C;
    if (name == "D") return reg::D;
    throw AssemblerError(lineNumber, "expected a register (A, B, C or D), got '" + text + "'");
}

// An address is either a number or the name of a label.
std::uint8_t parseAddress(const std::string& text, const LabelTable& labels, int lineNumber) {
    if (startsWithDigit(text)) {
        return parseNumber(text, lineNumber);
    }
    if (!isValidLabel(text)) {
        throw AssemblerError(lineNumber, "expected an address or label, got '" + text + "'");
    }
    auto found = labels.find(text);
    if (found == labels.end()) {
        throw AssemblerError(lineNumber, "undefined label '" + text + "'");
    }
    // A label after the last byte of a full 256-byte program would be 256.
    if (found->second > 255) {
        throw AssemblerError(lineNumber, "label '" + text + "' is outside memory");
    }
    return static_cast<std::uint8_t>(found->second);
}

std::uint8_t encodeOperand(OperandType type, const std::string& text,
                           const LabelTable& labels, int lineNumber) {
    switch (type) {
        case OperandType::Register:
            return parseRegister(text, lineNumber);
        case OperandType::Value:
            return parseNumber(text, lineNumber);
        case OperandType::Address:
            return parseAddress(text, labels, lineNumber);
    }
    return 0;  // unreachable: every OperandType is handled above
}

// ----- Pass 1 -----

// If the line starts with "name:", record the label and return the rest of
// the line (which may hold an instruction, or be empty).
std::string takeLabel(const std::string& line, int address, int lineNumber, LabelTable& labels) {
    std::size_t colon = line.find(':');
    if (colon == std::string::npos) {
        return line;
    }
    std::string name = trim(line.substr(0, colon));
    if (!isValidLabel(name)) {
        throw AssemblerError(lineNumber, "invalid label name '" + name + "'");
    }
    if (labels.count(name) != 0) {
        throw AssemblerError(lineNumber, "label '" + name + "' is defined more than once");
    }
    labels[name] = address;
    return trim(line.substr(colon + 1));
}

ParsedInstruction parseInstruction(const std::string& text, int lineNumber) {
    // The mnemonic ends at the first space or tab; the rest is the operands.
    std::size_t split = text.find_first_of(" \t");
    std::string mnemonic = toUpper(text.substr(0, split));
    std::string operandText = (split == std::string::npos) ? "" : text.substr(split + 1);

    const InstructionInfo* info = findInstructionByName(mnemonic);
    if (info == nullptr) {
        throw AssemblerError(lineNumber, "unknown instruction '" + mnemonic + "'");
    }

    std::vector<std::string> operands = splitOperands(operandText);
    if (operands.size() != info->operands.size()) {
        throw AssemblerError(lineNumber, info->name + " expects " +
                                             std::to_string(info->operands.size()) +
                                             " operand(s), got " + std::to_string(operands.size()));
    }
    for (const std::string& operand : operands) {
        if (operand.empty()) {
            throw AssemblerError(lineNumber, "missing operand");
        }
    }
    return {lineNumber, info, operands};
}

// Pass 1: find every instruction and record every label's address.
std::vector<ParsedInstruction> firstPass(const std::string& source, LabelTable& labels) {
    std::vector<ParsedInstruction> instructions;
    std::istringstream stream(source);
    std::string line;
    int lineNumber = 0;
    int address = 0;  // where the next instruction will go

    while (std::getline(stream, line)) {
        ++lineNumber;
        std::string text = trim(removeComment(line));
        text = takeLabel(text, address, lineNumber, labels);
        if (text.empty()) {
            continue;  // blank line, comment or label on its own
        }

        ParsedInstruction instruction = parseInstruction(text, lineNumber);
        address += 1 + static_cast<int>(instruction.operands.size());  // opcode + operands
        if (address > static_cast<int>(kMemorySize)) {
            throw AssemblerError(lineNumber, "program is larger than 256 bytes");
        }
        instructions.push_back(instruction);
    }
    return instructions;
}

}  // namespace

std::vector<std::uint8_t> assemble(const std::string& source) {
    LabelTable labels;
    std::vector<ParsedInstruction> instructions = firstPass(source, labels);

    // Pass 2: every label is now known, so each instruction can be encoded.
    std::vector<std::uint8_t> bytes;
    for (const ParsedInstruction& instruction : instructions) {
        bytes.push_back(instruction.info->opcode);
        for (std::size_t i = 0; i < instruction.operands.size(); ++i) {
            bytes.push_back(encodeOperand(instruction.info->operands[i], instruction.operands[i],
                                          labels, instruction.lineNumber));
        }
    }
    return bytes;
}

}  // namespace km8
