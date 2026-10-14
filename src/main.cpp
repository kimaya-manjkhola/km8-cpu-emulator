// main.cpp - the km8 command-line program.
//
// Usage: km8 run <file.asm> [--trace]
//
// Assembles the file, runs it on a fresh CPU, then prints the final
// registers, flags and cycle count. --trace also prints every instruction
// as it executes, with the register values after it.
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "km8/assembler.hpp"
#include "km8/cpu.hpp"
#include "km8/instruction_set.hpp"

using namespace km8;

namespace {

void printUsage() {
    std::cerr << "Usage: km8 run <file.asm> [--trace]\n";
}

// Read a whole file into a string. Returns false if it cannot be opened.
bool readFile(const std::string& path, std::string& contents) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    contents = buffer.str();
    return true;
}

// The four registers on one line, e.g. "A=10  B=0  C=0  D=0".
std::string registersLine(const Cpu& cpu) {
    std::ostringstream out;
    const char* names = "ABCD";
    for (std::uint8_t r = reg::A; r <= reg::D; ++r) {
        out << names[r] << '=' << std::left << std::setw(5) << int(cpu.registerValue(r));
    }
    return out.str();
}

void printTraceHeader() {
    std::cout << "cycle  addr  instruction     registers after               SP    Z C\n";
}

// One trace line: the instruction that just ran and the state it left behind.
void printTraceLine(const Cpu& cpu, std::uint8_t address, const std::string& instruction) {
    std::cout << std::right << std::setw(5) << cpu.cycles() << "  "
              << toHex(address) << "  "
              << std::left << std::setw(16) << instruction
              << registersLine(cpu) << "  "
              << toHex(cpu.sp()) << "  "
              << cpu.zeroFlag() << ' ' << cpu.carryFlag() << '\n';
}

// Run with a trace line per instruction. This repeats run()'s loop by hand
// because it needs to print between steps.
void runWithTrace(Cpu& cpu) {
    printTraceHeader();
    while (!cpu.isHalted() && cpu.cycles() < kDefaultMaxCycles) {
        std::uint8_t address = cpu.pc();
        // Format before stepping, while PC still points at the instruction.
        std::string instruction = formatInstruction(cpu.memory(), address);
        cpu.step();
        printTraceLine(cpu, address, instruction);
    }
    std::cout << '\n';
}

void printFinalState(const Cpu& cpu) {
    std::cout << "Registers:  " << registersLine(cpu) << '\n'
              << "PC:         " << toHex(cpu.pc()) << '\n'
              << "SP:         " << toHex(cpu.sp()) << '\n'
              << "Flags:      Z=" << cpu.zeroFlag() << "  C=" << cpu.carryFlag() << '\n'
              << "Cycles:     " << cpu.cycles() << '\n';
}

// Assemble and run one program. Returns the program's exit code.
int runFile(const std::string& path, bool trace) {
    std::string source;
    if (!readFile(path, source)) {
        std::cerr << "error: cannot open '" << path << "'\n";
        return 1;
    }

    Cpu cpu;
    try {
        cpu.loadProgram(assemble(source));
    } catch (const AssemblerError& error) {
        std::cerr << path << ": " << error.what() << '\n';
        return 1;
    }

    try {
        if (trace) {
            runWithTrace(cpu);
        } else {
            cpu.run();
        }
    } catch (const CpuError& error) {
        // Still show the state: it helps to see what the CPU was doing.
        std::cerr << "CPU error: " << error.what() << "\n\n";
        printFinalState(cpu);
        return 1;
    }

    if (!cpu.isHalted()) {
        std::cout << "Stopped: no HALT after " << kDefaultMaxCycles << " cycles (infinite loop?)\n\n";
        printFinalState(cpu);
        return 1;
    }
    std::cout << "Program halted.\n\n";
    printFinalState(cpu);
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    // Copy the arguments (skipping the program name) into easy-to-use strings.
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty() || args[0] != "run") {
        printUsage();
        return 1;
    }

    std::string path;
    bool trace = false;
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--trace") {
            trace = true;
        } else if (path.empty()) {
            path = args[i];
        } else {
            printUsage();  // more than one file given
            return 1;
        }
    }
    if (path.empty()) {
        printUsage();
        return 1;
    }
    return runFile(path, trace);
}
