// test_cpu_core.cpp - tests for CPU state, reset and the basic instructions.
#include <catch2/catch_test_macros.hpp>

#include "km8/cpu.hpp"

using namespace km8;

TEST_CASE("A new CPU starts in the power-on state", "[cpu]") {
    Cpu cpu;
    REQUIRE(cpu.pc() == 0);
    REQUIRE(cpu.sp() == 0);
    REQUIRE_FALSE(cpu.zeroFlag());
    REQUIRE_FALSE(cpu.carryFlag());
    REQUIRE_FALSE(cpu.isHalted());
    REQUIRE(cpu.cycles() == 0);
    for (std::uint8_t r = reg::A; r <= reg::D; ++r) {
        REQUIRE(cpu.registerValue(r) == 0);
    }
    for (std::uint8_t byte : cpu.memory()) {
        REQUIRE(byte == 0);
    }
}
