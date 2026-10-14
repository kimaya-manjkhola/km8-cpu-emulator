// cpu.cpp - implementation of the KM-8 CPU.
#include "km8/cpu.hpp"

namespace km8 {

Cpu::Cpu() {
    reset();
}

void Cpu::reset() {
    registers_.fill(0);
    memory_.fill(0);
    pc_ = 0;
    sp_ = 0;  // the first PUSH decrements to 255, so the stack starts at the top
    zero_ = false;
    carry_ = false;
    halted_ = false;
    cycles_ = 0;
}

std::uint8_t Cpu::registerValue(std::uint8_t code) const {
    // .at() checks the index, so a bad code in a test throws instead of
    // silently reading past the end of the array.
    return registers_.at(code);
}

}  // namespace km8
