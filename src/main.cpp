// vm6747 - run C6000 assembly as cc1i and cxx1i emit it for -arch tms6747.
//
//   vm6747 [-t] [-m megabytes] file.s [file2.s ...] [-- args]
//
// The files are assembled together, the C library and the EABI helpers are
// provided natively, main is called with argc and argv, and the exit status
// is main's return or exit's argument. -t traces every instruction.

#include "Asm.h"
#include "Cpu.h"
#include "Runtime.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    std::vector<std::string> files, args;
    bool trace = false;
    Layout layout;
    bool rest = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (rest) { args.push_back(a); continue; }
        if (a == "--") { rest = true; continue; }
        if (a == "-t") { trace = true; continue; }
        if (a == "-m" && i + 1 < argc) { layout.memoryBytes = static_cast<uint32_t>(std::atoi(argv[++i])) << 20; continue; }
        if (a == "-h" || a == "--help") {
            std::printf("usage: vm6747 [-t] [-m megabytes] file.s ... [-- args]\n");
            return 0;
        }
        files.push_back(a);
    }
    if (files.empty()) { std::fprintf(stderr, "vm6747: no input\n"); return 2; }

    layout.prelude = Runtime::prelude();

    Program prog;
    std::string error;
    if (!assemble(files, layout, Runtime::names(), prog, error)) {
        std::fprintf(stderr, "vm6747: %s\n", error.c_str());
        return 1;
    }
    std::map<std::string, uint32_t>::const_iterator m = prog.symbols.find("main");
    if (m == prog.symbols.end()) { std::fprintf(stderr, "vm6747: no main\n"); return 1; }

    Runtime rt;
    Cpu cpu(prog, rt);

    // The stack at the top of memory, argv just below it, the heap above the
    // program. B3 is where main returns to: the exit stub.
    uint32_t top = layout.memoryBytes - 16;
    std::vector<uint32_t> argPtrs;
    std::vector<std::string> all;
    all.push_back(files[0]);
    for (const std::string &a : args) all.push_back(a);
    for (const std::string &s : all) {
        top -= static_cast<uint32_t>(s.size()) + 1;
        cpu.writeBytes(top, s.c_str(), static_cast<uint32_t>(s.size()) + 1);
        argPtrs.push_back(top);
    }
    top &= ~7u;
    top -= 4 * (static_cast<uint32_t>(argPtrs.size()) + 1);
    top &= ~7u;
    uint32_t argvAt = top;
    for (size_t i = 0; i < argPtrs.size(); i++) cpu.store32(argvAt + 4 * static_cast<uint32_t>(i), argPtrs[i]);
    cpu.store32(argvAt + 4 * static_cast<uint32_t>(argPtrs.size()), 0);
    top -= 16;
    cpu.setReg(Cpu::B15, top);
    cpu.setReg(Cpu::A15, 0);
    cpu.setReg(Cpu::B3, 0xFC);
    cpu.setReg(Cpu::A4, static_cast<uint32_t>(argPtrs.size()));
    cpu.setReg(Cpu::B4, argvAt);
    rt.setHeap(prog.dataEnd, layout.memoryBytes / 2);

    // Dynamic initialisation before main: every .init_array entry, in order.
    for (size_t i = 0; i < prog.initArray.size(); i++)
        for (uint32_t a = prog.initArray[i].first; a < prog.initArray[i].first + prog.initArray[i].second; a += 4) {
            uint32_t fn = cpu.load32(a);
            if (fn != 0) cpu.callback(fn, 0, 0);
        }
    int status = cpu.run(m->second, trace);
    rt.runAtExit(cpu);
    status = cpu.exitCode();
    std::fflush(stdout);
    return status & 0xff;
}
