#pragma once

#include "Emitter.h"

#include <set>

namespace shalimar {

// The TMS320C6747 (C6000) as the Shalimar compiler's fourth target: C6000
// assembly text, serial, every delay slot a NOP - the conventions cc1i and
// cxx1i emit for it, which is what lets one emulator run all three and the
// runtime, compiled by cxx1i, sit beside a Shalimar program. See
// VM6747/TMS6747.md.
//
// The accumulator is A4, a real or a wide value the pair A5:A4. Slots are
// eight bytes at B15 + 8 + 8*slot, the saved return address at *B15; no
// frame pointer, since nothing here pushes. Arguments are positional, as the
// EABI has them - A4, B4, A6, B6, A8, B8, A10, B10, A12, B12, one register
// (or pair) per argument whatever its kind - and past ten the compiler's own
// overflow block, whose address travels in B1. A3 addresses, A0/A1 predicate.
class Tms6747Emitter : public Emitter {
public:
    std::string symbol(const std::string &name) const override { return name; }

    void beginModule(const std::string &sourceName) override;
    void endModule() override;

    void beginFunction(const std::string &name) override;
    void endFunction(int slots) override;

    void loadIntConstant(int32_t value) override;
    void loadRealConstant(double value) override;

    void storeSlot(Slot kind, int slot) override;
    void loadSlot(Slot kind, int slot) override;
    void setArg(Slot kind, int index) override;
    void loadSlotIntoArg(Slot kind, int slot, int index) override;

    bool positionalArguments() const override { return true; }
    int intArgCapacity() const override { return 10; }
    int realArgCapacity() const override { return 10; }
    void setOverflowBlock(int slot) override;
    void spillOverflowArgument(Slot kind, int index, int slot) override;
    void spillArgument(Slot kind, int registerIndex, int slot) override;
    void call(const std::string &name) override;
    void widenAccumulator() override;

    void loadSlotAddress(int slot) override;
    void storeThroughPointer(Slot kind, int pointerSlot) override;
    void loadThroughPointer(Slot kind, int pointerSlot) override;

    void defineGlobals(int slots) override;
    void loadGlobal(Slot kind, int index) override;
    void storeGlobal(Slot kind, int index) override;

    void defineBytes(int id, const std::string &bytes) override;
    void loadBytesAddress(int id) override;

    void label(int id) override;
    void jump(int id) override;
    void jumpIfZero(int id) override;

private:
    size_t prologueMark_ = 0;
    int returns_ = 0;
    int globalSlots_ = 0;
    // The TI assembler wants an undefined name declared: what was called,
    // less what this file defines, is `.ref`ed when the module ends.
    std::set<std::string> called_, defined_;

    // A constant into a register, MVKL then MVKH - the only way to a 32-bit value.
    void constant(const std::string &reg, int32_t value);
    // The address of a slot into `reg`: B15 + 8 + 8*slot.
    void slotAddress(int slot, const std::string &reg);
    // A load or store of `kind` at *reg, into or from the accumulator or a given register.
    void loadAt(Slot kind, const std::string &addrReg, const std::string &reg);
    void storeAt(Slot kind, const std::string &addrReg, const std::string &reg);
    // The register a positional argument rides in: A4, B4, A6, ...
    static std::string argRegister(int index);
    static std::string pairOf(const std::string &reg);

    static std::string labelName(int id);
    static std::string bytesLabel(int id);
};

}
