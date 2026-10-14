// cpu.hpp - the KM-8 CPU: registers, flags, memory and the
// fetch-decode-execute cycle. See docs/SPEC.md for the design.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace km8 {

// Addresses are 8 bits wide, so the whole address space is 2^8 = 256 bytes.
constexpr std::size_t kMemorySize = 256;
constexpr std::size_t kNumRegisters = 4;

// Register codes, exactly as they appear in machine code.
namespace reg {
constexpr std::uint8_t A = 0;
constexpr std::uint8_t B = 1;
constexpr std::uint8_t C = 2;
constexpr std::uint8_t D = 3;
}  // namespace reg

class Cpu {
public:
    Cpu();

    // Put the CPU back to its power-on state: everything zero, not halted.
    void reset();

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
    std::array<std::uint8_t, kNumRegisters> registers_{};  // A, B, C, D
    std::array<std::uint8_t, kMemorySize> memory_{};
    std::uint8_t pc_ = 0;
    std::uint8_t sp_ = 0;
    bool zero_ = false;    // Z flag
    bool carry_ = false;   // C flag
    bool halted_ = false;
    std::uint64_t cycles_ = 0;  // instructions executed since reset
};

}  // namespace km8
