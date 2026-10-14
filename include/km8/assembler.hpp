// assembler.hpp - turns KM-8 assembly text into machine-code bytes.
//
// Syntax (one instruction per line):
//     loop:            ; a label marks the address of the next instruction
//         LDI A, 10    ; numbers can be decimal (10) or hex (0x0A)
//         DEC A
//         JNZ loop     ; labels can be used wherever an address is expected
//         HALT
//
// Instruction and register names are not case-sensitive; labels are.
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace km8 {

// Thrown for any problem in the source text. The message starts with the
// line number, e.g. "line 3: unknown instruction 'FOO'".
class AssemblerError : public std::runtime_error {
public:
    AssemblerError(int lineNumber, const std::string& message);
    int lineNumber() const { return lineNumber_; }

private:
    int lineNumber_;
};

// Assemble a whole program. Throws AssemblerError on the first problem found.
std::vector<std::uint8_t> assemble(const std::string& source);

}  // namespace km8
