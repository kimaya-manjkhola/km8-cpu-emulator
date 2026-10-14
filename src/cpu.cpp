// cpu.cpp - implementation of the KM-8 CPU.
#include "km8/cpu.hpp"

#include "km8/opcodes.hpp"

namespace km8 {

std::string toHex(std::uint8_t value) {
    const char* digits = "0123456789ABCDEF";
    std::string text = "0x";
    text += digits[value >> 4];   // high nibble
    text += digits[value & 0x0F]; // low nibble
    return text;
}

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
    instructionAddress_ = 0;
}

void Cpu::loadProgram(const std::vector<std::uint8_t>& program) {
    if (program.size() > kMemorySize) {
        throw CpuError("program is " + std::to_string(program.size()) +
                       " bytes but memory is only " + std::to_string(kMemorySize) + " bytes");
    }
    reset();
    for (std::size_t i = 0; i < program.size(); ++i) {
        memory_[i] = program[i];
    }
}

void Cpu::step() {
    if (halted_) {
        return;  // a halted CPU stays halted until reset()
    }
    instructionAddress_ = pc_;  // remembered for error messages
    std::uint8_t opcode = fetchByte();
    execute(opcode);
    ++cycles_;
}

StopReason Cpu::run(std::uint64_t maxCycles) {
    for (std::uint64_t i = 0; i < maxCycles && !halted_; ++i) {
        step();
    }
    return halted_ ? StopReason::Halted : StopReason::CycleLimit;
}

std::uint8_t Cpu::registerValue(std::uint8_t code) const {
    // .at() checks the index, so a bad code in a test throws instead of
    // silently reading past the end of the array.
    return registers_.at(code);
}

std::uint8_t Cpu::fetchByte() {
    // pc_ is 8-bit, so incrementing past 255 wraps to 0 just like hardware.
    std::uint8_t byte = memory_[pc_];
    ++pc_;
    return byte;
}

std::uint8_t& Cpu::fetchRegister() {
    std::uint8_t code = fetchByte();
    if (code >= kNumRegisters) {
        fail("invalid register code " + std::to_string(code));
    }
    return registers_[code];
}

void Cpu::fail(const std::string& message) {
    halted_ = true;  // the spec says a bad instruction stops the CPU
    throw CpuError(message + " at address " + toHex(instructionAddress_));
}

void Cpu::execute(std::uint8_t opcode) {
    // The decoder: one case per opcode, like a case statement in Verilog.
    // Operands are always fetched in the order they appear in memory.
    switch (opcode) {
        case op::HALT:
            halted_ = true;
            break;

        case op::NOP:
            break;

        case op::LDI: {  // LDI r, value
            std::uint8_t& r = fetchRegister();
            r = fetchByte();
            break;
        }

        case op::LOAD: {  // LOAD r, addr
            std::uint8_t& r = fetchRegister();
            r = memory_[fetchByte()];
            break;
        }

        case op::STORE: {  // STORE r, addr
            std::uint8_t value = fetchRegister();
            memory_[fetchByte()] = value;
            break;
        }

        case op::MOV: {  // MOV r1, r2
            std::uint8_t& destination = fetchRegister();
            destination = fetchRegister();
            break;
        }

        default:
            fail("unknown opcode " + toHex(opcode));
    }
}

}  // namespace km8
