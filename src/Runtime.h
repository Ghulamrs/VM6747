#pragma once

// The C library and the EABI helpers, provided natively: a call to one of
// these names lands on a stub below the text base and is answered here,
// reading its arguments by the C6000 convention the compilers emit - A4, B4,
// A6 ... then the stack from B15+4, and for a variadic function the last
// named argument and everything after it on the stack, 8-byte values at an
// 8-byte boundary - and returning in A4 (A5:A4). Memory the library hands
// out comes from a heap above the program's data.

#include <cstdint>
#include <string>
#include <vector>

class Cpu;

class Runtime {
public:
    static std::vector<std::string> names();
    bool call(const std::string &name, Cpu &cpu);
    void setHeap(uint32_t start, uint32_t end) { heap_ = start; heapEnd_ = end; }

private:
    uint32_t heap_ = 0, heapEnd_ = 0;
    struct Block { uint32_t at, size; bool used; };
    std::vector<Block> blocks_;
    uint32_t allocate(Cpu &cpu, uint32_t size);
    void release(Cpu &cpu, uint32_t at);

    // A cursor over arguments in memory, as va_list sees them.
    struct Args {
        Cpu &cpu; uint32_t at;
        uint32_t word();
        uint64_t wide();
        double dbl();
    };
    std::string format(Cpu &cpu, const std::string &fmt, Args &args);
    struct File { std::string path; std::string data; size_t pos; bool write; };
    std::vector<File> files_;
    uint32_t handlers_[32] = { 0 };
    uint32_t errno_ = 0;
    uint32_t errnoAt(Cpu &cpu);
    void setErrno(Cpu &cpu, uint32_t v);
};
