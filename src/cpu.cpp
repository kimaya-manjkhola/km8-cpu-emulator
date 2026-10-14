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

std::uint8_t Cpu::addWithFlags(std::uint8_t a, std::uint8_t b) {
    // Add in a wider type so the 9th bit (the carry out) is not lost.
    unsigned int sum = a + b;
    carry_ = sum > 0xFF;
    std::uint8_t result = static_cast<std::uint8_t>(sum);  // keep the low 8 bits
    zero_ = (result == 0);
    return result;
}

std::uint8_t Cpu::subtractWithFlags(std::uint8_t a, std::uint8_t b) {
    carry_ = b > a;  // a borrow is needed when subtracting a bigger number
    // Unsigned arithmetic wraps modulo 256, so 0 - 1 gives 255.
    std::uint8_t result = static_cast<std::uint8_t>(a - b);
    zero_ = (result == 0);
    return result;
}

void Cpu::setLogicFlags(std::uint8_t result) {
    zero_ = (result == 0);
    carry_ = false;  // logic operations can never carry, so C is cleared
}

void Cpu::jumpIf(bool condition) {
    // The target is always fetched, even when the jump is not taken, so that
    // PC ends up on the next instruction either way.
    std::uint8_t target = fetchByte();
    if (condition) {
        pc_ = target;
    }
}

void Cpu::push(std::uint8_t value) {
    // Decrement first, then write: SP starts at 0, so the first push wraps SP
    // to 255 and uses the top byte of memory. The stack grows downwards.
    --sp_;
    memory_[sp_] = value;
}

std::uint8_t Cpu::pop() {
    // The exact reverse of push(): read first, then increment.
    std::uint8_t value = memory_[sp_];
    ++sp_;
    return value;
}

void Cpu::execute(std::uint8_t opcode) {
    // The decoder: one case per opcode, like a case statement in Verilog.
    // Operands are always fetched in the order they appear in memory.
    switch (opcode) {
        // ----- Data movement (these never change the flags) -----
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

        // LDR and STR use a register as a pointer: the address comes from a
        // register at run time, not from a fixed operand byte. This is what
        // lets a loop step through an array by incrementing the pointer.
        case op::LDR: {  // LDR r1, r2 - r1 = memory[r2]
            std::uint8_t& r1 = fetchRegister();
            std::uint8_t address = fetchRegister();
            r1 = memory_[address];
            break;
        }

        case op::STR: {  // STR r1, r2 - memory[r2] = r1
            std::uint8_t value = fetchRegister();
            std::uint8_t address = fetchRegister();
            memory_[address] = value;
            break;
        }

        // ----- Arithmetic -----
        case op::ADD: {  // ADD r1, r2
            std::uint8_t& r1 = fetchRegister();
            std::uint8_t r2 = fetchRegister();
            r1 = addWithFlags(r1, r2);
            break;
        }

        case op::SUB: {  // SUB r1, r2
            std::uint8_t& r1 = fetchRegister();
            std::uint8_t r2 = fetchRegister();
            r1 = subtractWithFlags(r1, r2);
            break;
        }

        case op::INC: {  // INC r - only Z changes; C keeps its old value
            std::uint8_t& r = fetchRegister();
            ++r;
            zero_ = (r == 0);
            break;
        }

        case op::DEC: {  // DEC r - only Z changes; C keeps its old value
            std::uint8_t& r = fetchRegister();
            --r;
            zero_ = (r == 0);
            break;
        }

        // ----- Logic -----
        case op::AND: {  // AND r1, r2
            std::uint8_t& r1 = fetchRegister();
            r1 &= fetchRegister();
            setLogicFlags(r1);
            break;
        }

        case op::OR: {  // OR r1, r2
            std::uint8_t& r1 = fetchRegister();
            r1 |= fetchRegister();
            setLogicFlags(r1);
            break;
        }

        case op::XOR: {  // XOR r1, r2
            std::uint8_t& r1 = fetchRegister();
            r1 ^= fetchRegister();
            setLogicFlags(r1);
            break;
        }

        // ----- Compare -----
        case op::CMP: {  // CMP r1, r2 - subtract only for the flags
            std::uint8_t r1 = fetchRegister();
            std::uint8_t r2 = fetchRegister();
            subtractWithFlags(r1, r2);  // result deliberately thrown away
            break;
        }

        // ----- Jumps -----
        case op::JMP:
            jumpIf(true);
            break;

        case op::JZ:
            jumpIf(zero_);
            break;

        case op::JNZ:
            jumpIf(!zero_);
            break;

        case op::JC:
            jumpIf(carry_);
            break;

        // ----- Stack and subroutines -----
        case op::PUSH:  // PUSH r
            push(fetchRegister());
            break;

        case op::POP: {  // POP r
            std::uint8_t& r = fetchRegister();
            r = pop();
            break;
        }

        case op::CALL: {  // CALL addr
            std::uint8_t target = fetchByte();
            // After fetching the operand, PC already points at the next
            // instruction, which is exactly the return address we need.
            push(pc_);
            pc_ = target;
            break;
        }

        case op::RET:
            pc_ = pop();
            break;

        default:
            fail("unknown opcode " + toHex(opcode));
    }
}

}  // namespace km8
