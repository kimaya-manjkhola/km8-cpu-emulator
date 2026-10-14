// test_helpers.hpp - small helpers shared by the test files.
#pragma once

#include <cstdint>
#include <vector>

#include "km8/cpu.hpp"

namespace km8::test {

// Load a program, run it until HALT (or the cycle limit), and return the CPU
// so the test can inspect the final state.
inline Cpu runProgram(const std::vector<std::uint8_t>& program,
                      std::uint64_t maxCycles = kDefaultMaxCycles) {
    Cpu cpu;
    cpu.loadProgram(program);
    cpu.run(maxCycles);
    return cpu;
}

}  // namespace km8::test
