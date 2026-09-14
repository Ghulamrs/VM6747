#include "Tms6747.h"

#include <cstring>

namespace shalimar {

void Tms6747Emitter::beginModule(const std::string &sourceName) {
    raw("; " + sourceName);
    raw("\t.text");
}

void Tms6747Emitter::endModule() {
    if (globalSlots_ > 0) {
        blank();
        raw("\t.global\t" + symbol("shm_globals"));
        raw("\t.bss\t" + symbol("shm_globals") + ", " +
            std::to_string(globalSlots_ * 8) + ", 8");
    }
    if (!data_.empty()) {
        blank();
        raw("\t.sect\t\".const\"");
        text_ += data_;
    }
    for (const std::string &name : called_)
        if (!defined_.count(name)) raw("\t.ref\t" + symbol(name));
    blank();
}

void Tms6747Emitter::constant(const std::string &reg, int32_t value) {
    instruction("MVKL\t" + std::to_string(value) + ", " + reg);
    instruction("MVKH\t" + std::to_string(value) + ", " + reg);
}

void Tms6747Emitter::slotAddress(int slot, const std::string &reg) {
    constant(reg, 8 + 8 * slot);
    instruction("ADD\tB15, " + reg + ", " + reg);
}

std::string Tms6747Emitter::pairOf(const std::string &reg) {
    // "A4" -> "A5:A4"
    const int n = std::atoi(reg.c_str() + 1);
    return reg.substr(0, 1) + std::to_string(n + 1) + ":" + reg;
}

std::string Tms6747Emitter::argRegister(int index) {
    const int number = 4 + 2 * (index / 2);
    return std::string(index % 2 == 0 ? "A" : "B") + std::to_string(number);
}

void Tms6747Emitter::loadAt(Slot kind, const std::string &addrReg, const std::string &reg) {
    if (kind == Slot::Int) instruction("LDW\t*" + addrReg + ", " + reg);
    else                   instruction("LDDW\t*" + addrReg + ", " + pairOf(reg));
    instruction("NOP\t4");
}

void Tms6747Emitter::storeAt(Slot kind, const std::string &addrReg, const std::string &reg) {
    if (kind == Slot::Int) instruction("STW\t" + reg + ", *" + addrReg);
    else                   instruction("STDW\t" + pairOf(reg) + ", *" + addrReg);
}

std::string Tms6747Emitter::labelName(int id) { return "Lshm" + std::to_string(id); }
std::string Tms6747Emitter::bytesLabel(int id) { return "Lshmb" + std::to_string(id); }

void Tms6747Emitter::beginFunction(const std::string &name) {
    blank();
    raw("\t.global\t" + symbol(name));
    raw(symbol(name) + ":");
    defined_.insert(name);
    usesSavedArgRegs_ = false;
    prologueMark_ = text_.size();
}

// The frame: the return address at *B15, the slots above it, eight bytes
// each, the whole a multiple of eight so that B15 stays 8-aligned and every
// LDDW/STDW lands on an aligned pair.
void Tms6747Emitter::endFunction(int slots) {
    static const char *const kept[] = { "A10", "B10", "A12", "B12" };
    const int saved = 8 + 8 * slots;                  // where the four kept registers go
    const int frame = saved + (usesSavedArgRegs_ ? 16 : 0);
    std::string prologue;
    prologue += "\tMVKL\t" + std::to_string(frame) + ", A3\n";
    prologue += "\tMVKH\t" + std::to_string(frame) + ", A3\n";
    prologue += "\tSUB\tB15, A3, B15\n";
    prologue += "\tSTW\tB3, *B15\n";
    if (usesSavedArgRegs_)
        for (int k = 0; k < 4; k++)
            prologue += "\tSTW\t" + std::string(kept[k]) + ", *+B15(" + std::to_string(saved + 4 * k) + ")\n";
    text_.insert(prologueMark_, prologue);

    if (usesSavedArgRegs_)
        for (int k = 0; k < 4; k++)
            instruction("LDW\t*+B15(" + std::to_string(saved + 4 * k) + "), " + kept[k]);
    instruction("LDW\t*B15, B3");
    instruction("NOP\t4");
    constant("A3", frame);
    instruction("ADD\tB15, A3, B15");
    instruction("B\tB3");
    instruction("NOP\t5");
}

void Tms6747Emitter::loadIntConstant(int32_t value) {
    constant("A4", value);
}

void Tms6747Emitter::loadRealConstant(double value) {
    const uint64_t bits = bitsOf(value);
    constant("A4", static_cast<int32_t>(static_cast<uint32_t>(bits & 0xFFFFFFFFu)));
    constant("A5", static_cast<int32_t>(static_cast<uint32_t>(bits >> 32)));
}

void Tms6747Emitter::storeSlot(Slot kind, int slot) {
    slotAddress(slot, "A3");
    storeAt(kind, "A3", "A4");
}

void Tms6747Emitter::loadSlot(Slot kind, int slot) {
    slotAddress(slot, "A3");
    loadAt(kind, "A3", "A4");
}

void Tms6747Emitter::setArg(Slot kind, int index) {
    if (index == 0) return;
    if (index >= 6) usesSavedArgRegs_ = true;
    const std::string reg = argRegister(index);
    instruction("MV\tA4, " + reg);
    if (kind != Slot::Int) instruction("MV\tA5, " + pairOf(reg).substr(0, pairOf(reg).find(':')));
}

void Tms6747Emitter::loadSlotIntoArg(Slot kind, int slot, int index) {
    if (index >= 6) usesSavedArgRegs_ = true;
    slotAddress(slot, "A3");
    loadAt(kind, "A3", argRegister(index));
}

void Tms6747Emitter::spillArgument(Slot kind, int registerIndex, int slot) {
    slotAddress(slot, "A3");
    storeAt(kind, "A3", argRegister(registerIndex));
}

// The overflow block: the caller's slots holding the arguments past ten,
// their address handed over in B1 - a convention between functions this
// compiler writes; no runtime function takes that many.
void Tms6747Emitter::setOverflowBlock(int slot) {
    slotAddress(slot, "B1");
}

void Tms6747Emitter::spillOverflowArgument(Slot kind, int index, int slot) {
    constant("A6", 8 * index);
    instruction("ADD\tB1, A6, A6");
    loadAt(kind, "A6", "A8");
    slotAddress(slot, "A3");
    storeAt(kind, "A3", "A8");
}

void Tms6747Emitter::call(const std::string &name) {
    const std::string back = "Lshmr" + std::to_string(returns_++);
    instruction("MVKL\t" + back + ", B3");
    instruction("MVKH\t" + back + ", B3");
    instruction("B\t" + symbol(name));
    instruction("NOP\t5");
    raw(back + ":");
    called_.insert(name);
}

void Tms6747Emitter::loadSlotAddress(int slot) {
    slotAddress(slot, "A4");
}

void Tms6747Emitter::storeThroughPointer(Slot kind, int pointerSlot) {
    slotAddress(pointerSlot, "A3");
    instruction("LDW\t*A3, A3");
    instruction("NOP\t4");
    storeAt(kind, "A3", "A4");
}

void Tms6747Emitter::loadThroughPointer(Slot kind, int pointerSlot) {
    slotAddress(pointerSlot, "A3");
    instruction("LDW\t*A3, A3");
    instruction("NOP\t4");
    loadAt(kind, "A3", "A4");
}

void Tms6747Emitter::defineGlobals(int slots) {
    globalSlots_ = slots;
}

void Tms6747Emitter::loadGlobal(Slot kind, int index) {
    instruction("MVKL\t" + symbol("shm_globals") + ", A3");
    instruction("MVKH\t" + symbol("shm_globals") + ", A3");
    constant("A6", 8 * index);
    instruction("ADD\tA3, A6, A3");
    loadAt(kind, "A3", "A4");
}

void Tms6747Emitter::storeGlobal(Slot kind, int index) {
    instruction("MVKL\t" + symbol("shm_globals") + ", A3");
    instruction("MVKH\t" + symbol("shm_globals") + ", A3");
    constant("A6", 8 * index);
    instruction("ADD\tA3, A6, A3");
    storeAt(kind, "A3", "A4");
}

void Tms6747Emitter::defineBytes(int id, const std::string &bytes) {
    data_ += bytesLabel(id) + ":\n";
    std::string line;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (line.empty()) line = "\t.byte\t";
        else line += ", ";
        line += std::to_string(static_cast<unsigned char>(bytes[i]));
        if (i % 16 == 15) { data_ += line + "\n"; line.clear(); }
    }
    if (!line.empty()) data_ += line + "\n";
}

void Tms6747Emitter::loadBytesAddress(int id) {
    instruction("MVKL\t" + bytesLabel(id) + ", A4");
    instruction("MVKH\t" + bytesLabel(id) + ", A4");
}

// An int made wide: the sign copied into the high word of the pair.
void Tms6747Emitter::widenAccumulator() {
    instruction("SHR\tA4, 31, A5");
}

void Tms6747Emitter::label(int id) {
    raw(labelName(id) + ":");
}

void Tms6747Emitter::jump(int id) {
    instruction("B\t" + labelName(id));
    instruction("NOP\t5");
}

void Tms6747Emitter::jumpIfZero(int id) {
    instruction("MV\tA4, A1");
    instruction("[!A1]\tB\t" + labelName(id));
    instruction("NOP\t5");
}

}
