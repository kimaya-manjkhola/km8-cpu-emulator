// cpu.hpp - the KM-8 CPU: registers, flags, memory and the
// fetch-decode-execute cycle. See docs/SPEC.md for the design.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace km8 {

// Addresses are 8 bits wide, so the whole address space is 2^8 = 256 bytes.
constexpr std::size_t kMemorySize = 256;
constexpr std::size_t kNumRegisters = 4;

// Default cycle limit for run(). One cycle = one instruction. The limit
// exists so that a program stuck in an infinite loop cannot hang the emulator.
constexpr std::uint64_t kDefaultMaxCycles = 10000;

// Register codes, exactly as they appear in machine code.
namespace reg {
constexpr std::uint8_t A = 0;
constexpr std::uint8_t B = 1;
constexpr std::uint8_t C = 2;
constexpr std::uint8_t D = 3;
}  // namespace reg

// Thrown when the CPU meets something it cannot execute (an unknown opcode
// or an invalid register code), or when a program is too big to load.
class CpuError : public std::runtime_error {
public:
    explicit CpuError(const std::string& message) : std::runtime_error(message) {}
};

// Why run() stopped. Hitting the cycle limit is a normal outcome, not an
// error, so it is reported as a return value rather than an exception.
enum class StopReason {
    Halted,      // the program executed HALT
    CycleLimit,  // maxCycles instructions ran without reaching HALT
};

// Format a byte as "0x" followed by two hex digits, e.g. 0x0A.
// Used in error messages and in the trace output.
std::string toHex(std::uint8_t value);

class Cpu {
public:
    Cpu();

    // Put the CPU back to its power-on state: everything zero, not halted.
    void reset();

    // Reset the CPU, then copy the program into memory starting at address 0.
    // Throws CpuError if the program is larger than memory.
    void loadProgram(const std::vector<std::uint8_t>& program);

    // Run one fetch-decode-execute cycle (one whole instruction).
    // Does nothing if the CPU is halted. Throws CpuError on a bad instruction.
    void step();

    // Call step() until HALT, or until maxCycles instructions have run.
    StopReason run(std::uint64_t maxCycles = kDefaultMaxCycles);

    // Read-only accessors. Tests and the command-line program use these to
    // inspect the machine; nothing outside the class can change its state.
    std::uint8_t registerValue(std::uint8_t code) const;
    std::uint8_t pc() const { return pc_; }
    std::uint8_t sp() const { return sp_; }
    bool zeroFlag() const { return zero_; }
    bool carryFlag() const { return carry_; }
    bool isHalted() const { return halted_; }
    std::uint64_t cycles() const { return cycles_; }
    const std::array<std::uint8_t, kMemorySize>& memory() const { return memory_; }

private:
    // Fetch helpers: read the byte at PC and move PC on by one.
    std::uint8_t fetchByte();
    std::uint8_t& fetchRegister();  // returns a reference so it can be written

    // Decode the opcode and carry out the instruction.
    void execute(std::uint8_t opcode);

    // ALU helpers: compute a result and set the flags as docs/SPEC.md says.
    std::uint8_t addWithFlags(std::uint8_t a, std::uint8_t b);
    std::uint8_t subtractWithFlags(std::uint8_t a, std::uint8_t b);
    void setLogicFlags(std::uint8_t result);

    // Read the jump target operand, then jump to it only if condition is true.
    void jumpIf(bool condition);

    // Stack helpers, shared by PUSH/POP and CALL/RET.
    void push(std::uint8_t value);
    std::uint8_t pop();

    // Stop the CPU and throw a CpuError that says where it happened.
    [[noreturn]] void fail(const std::string& message);

    std::array<std::uint8_t, kNumRegisters> registers_{};  // A, B, C, D
    std::array<std::uint8_t, kMemorySize> memory_{};
    std::uint8_t pc_ = 0;
    std::uint8_t sp_ = 0;
    bool zero_ = false;    // Z flag
    bool carry_ = false;   // C flag
    bool halted_ = false;
    std::uint64_t cycles_ = 0;           // instructions executed since reset
    std::uint8_t instructionAddress_ = 0;  // where the current instruction began
};

}  // namespace km8
