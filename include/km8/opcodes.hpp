// opcodes.hpp - the opcode byte for every KM-8 instruction (see docs/SPEC.md).
//
// These are named constants rather than "magic numbers", so the CPU's
// decoder and the tests read like assembly: op::ADD instead of 0x10.
#pragma once

#include <cstdint>

namespace km8::op {

// Data movement
constexpr std::uint8_t HALT  = 0x00;
constexpr std::uint8_t NOP   = 0x01;
constexpr std::uint8_t LDI   = 0x02;
constexpr std::uint8_t LOAD  = 0x03;
constexpr std::uint8_t STORE = 0x04;
constexpr std::uint8_t MOV   = 0x05;
constexpr std::uint8_t LDR   = 0x06;  // register-indirect load
constexpr std::uint8_t STR   = 0x07;  // register-indirect store

// Arithmetic
constexpr std::uint8_t ADD = 0x10;
constexpr std::uint8_t SUB = 0x11;
constexpr std::uint8_t INC = 0x12;
constexpr std::uint8_t DEC = 0x13;

// Logic
constexpr std::uint8_t AND = 0x20;
constexpr std::uint8_t OR  = 0x21;
constexpr std::uint8_t XOR = 0x22;

// Compare
constexpr std::uint8_t CMP = 0x30;

// Jumps
constexpr std::uint8_t JMP = 0x40;
constexpr std::uint8_t JZ  = 0x41;
constexpr std::uint8_t JNZ = 0x42;
constexpr std::uint8_t JC  = 0x43;

// Stack and subroutines
constexpr std::uint8_t PUSH = 0x50;
constexpr std::uint8_t POP  = 0x51;
constexpr std::uint8_t CALL = 0x52;
constexpr std::uint8_t RET  = 0x53;

}  // namespace km8::op
