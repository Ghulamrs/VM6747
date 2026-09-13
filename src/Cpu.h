#pragma once

// The C674x as this emulator models it: the two register files, a program
// counter, and the pipeline's one visible property - a result lands some
// cycles after its instruction issues, and a branch takes effect five packets
// after it - which is what the NOPs in the emitted code are for, and what a
// too-short NOP would get wrong. Registers read the old value until the
// cycle a write lands; memory effects are immediate at issue, which keeps a
// store and a later load in order.
//
// Execution is by parsed operands, one execute packet per cycle, parallel
// (||) instructions reading their operands before any of them writes. A PC
// below the text base is a native stub: the runtime answers it and returns
// through B3.

#include "Program.h"

#include <cstdint>
#include <string>
#include <vector>

class Runtime;

class Cpu {
public:
    Cpu(Program &prog, Runtime &rt);

    // Run from main until exit; returns the exit status.
    int run(uint32_t entry, bool trace);

    // What the runtime needs.
    uint32_t reg(int r) const { return r_[r]; }
    void setReg(int r, uint32_t v) { r_[r] = v; }
    uint64_t pair(int lo) const { return (uint64_t(r_[lo + 1]) << 32) | r_[lo]; }
    void setPair(int lo, uint64_t v) { r_[lo] = uint32_t(v); r_[lo + 1] = uint32_t(v >> 32); }
    uint8_t load8(uint32_t a);
    uint16_t load16(uint32_t a);
    uint32_t load32(uint32_t a);
    uint64_t load64(uint32_t a);
    void store8(uint32_t a, uint8_t v);
    void store16(uint32_t a, uint16_t v);
    void store32(uint32_t a, uint32_t v);
    void store64(uint32_t a, uint64_t v);
    std::string readString(uint32_t a);
    void writeBytes(uint32_t a, const void *p, uint32_t n);
    void exitWith(int code) { running_ = false; exitCode_ = code; }
    void resume() { running_ = true; }   // to run atexit handlers after exit
    int exitCode() const { return exitCode_; }
    void jumpTo(uint32_t target) { pc_ = target; }   // for longjmp: immediate
    // Call a function in the program from the runtime (qsort's comparator):
    // A4 and B4 as arguments, A4 back. Runs nested until it returns.
    uint32_t callback(uint32_t fn, uint32_t a4, uint32_t b4);
    [[noreturn]] void fault(const std::string &what);
    Program &program() { return prog_; }
    uint64_t cycle() const { return cycle_; }
    std::string where(uint32_t pc) const;

    static const int A = 0, B = 16;
    static const int A4 = 4, B3 = 16 + 3, B4 = 16 + 4, B15 = 16 + 15, A15 = 15;

private:
    Program &prog_;
    Runtime &rt_;
    uint32_t r_[32];
    uint32_t pc_ = 0;
    uint64_t cycle_ = 0;
    bool running_ = true;
    int exitCode_ = 0;
    bool trace_ = false;

    struct Pending { uint64_t at; int reg; uint32_t value; };
    std::vector<Pending> pending_;
    bool branchValid_ = false;
    uint64_t branchAt_ = 0;
    uint32_t branchTarget_ = 0;

    void step();
    void applyPending();
    void tick();                       // one idle cycle
    void executePacket();
    void execute(const Instr &in, std::vector<Pending> &writes, bool &branched, uint32_t &target);
    void write(std::vector<Pending> &w, int reg, uint32_t v, int delay);
    void writePair(std::vector<Pending> &w, int lo, uint64_t v, int delay);
    uint32_t value(const Instr &in, const Operand &o);
    uint32_t address(const Instr &in, const Operand &o, int size, std::vector<Pending> &w);
    void check(uint32_t a, int size, bool aligned);
};
