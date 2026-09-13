#include "Runtime.h"
#include "Cpu.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// ---- the argument cursor ------------------------------------------------------
uint32_t Runtime::Args::word() { uint32_t v = cpu.load32(at); at += 4; return v; }
uint64_t Runtime::Args::wide() { at = (at + 7) & ~7u; uint64_t v = cpu.load64(at); at += 8; return v; }
double Runtime::Args::dbl() { uint64_t v = wide(); double d; std::memcpy(&d, &v, 8); return d; }

// The registers an argument arrives in, by position, and where the stack
// part begins: B15 + 4 is the first stack word.
static const int kArgRegs[10] = { 4, 16 + 4, 6, 16 + 6, 8, 16 + 8, 10, 16 + 10, 12, 16 + 12 };
static uint32_t arg(Cpu &c, int i) {
    if (i < 10) return c.reg(kArgRegs[i]);
    return c.load32(c.reg(Cpu::B15) + 4 + 4 * (i - 10));
}
static uint64_t argWide(Cpu &c, int i) {
    if (i < 10) return c.pair(kArgRegs[i]);
    return c.load64(c.reg(Cpu::B15) + 4 + 4 * (i - 10));
}
static double argDouble(Cpu &c, int i) { uint64_t v = argWide(c, i); double d; std::memcpy(&d, &v, 8); return d; }
static float argFloat(Cpu &c, int i) { uint32_t v = arg(c, i); float f; std::memcpy(&f, &v, 4); return f; }
// A variadic call: the last named argument is on the stack; this is the
// cursor at it, given how many named arguments there are.
static uint32_t variadicAt(Cpu &c, int named) {
    return c.reg(Cpu::B15) + 4;
    (void)named;
}
static void ret(Cpu &c, uint32_t v) { c.setReg(Cpu::A4, v); }
static void retWide(Cpu &c, uint64_t v) { c.setPair(Cpu::A4, v); }
// A NaN goes back as the canonical quiet NaN, sign clear, as the CPU's own
// arithmetic answers it - not as the host happened to produce it.
static void retDouble(Cpu &c, double d) { uint64_t v; if (d != d) v = 0x7ff8000000000000ULL; else std::memcpy(&v, &d, 8); retWide(c, v); }
static void retFloat(Cpu &c, float f) { uint32_t v; if (f != f) v = 0x7fc00000u; else std::memcpy(&v, &f, 4); ret(c, v); }

// ---- errno: one word on the heap, handed out by address -----------------------
uint32_t Runtime::errnoAt(Cpu &cpu) {
    if (errno_ == 0) errno_ = allocate(cpu, 4);
    return errno_;
}
void Runtime::setErrno(Cpu &cpu, uint32_t v) { cpu.store32(errnoAt(cpu), v); }

// ---- the heap ------------------------------------------------------------------
uint32_t Runtime::allocate(Cpu &cpu, uint32_t size) {
    if (size == 0) size = 1;
    size = (size + 7) & ~7u;
    for (Block &b : blocks_)
        if (!b.used && b.size >= size) { b.used = true; return b.at; }
    if (heap_ + size > heapEnd_) cpu.fault("the heap is exhausted");
    Block b; b.at = heap_; b.size = size; b.used = true;
    blocks_.push_back(b);
    heap_ += size;
    return b.at;
}
void Runtime::release(Cpu &cpu, uint32_t at) {
    if (at == 0) return;
    for (Block &b : blocks_) if (b.at == at) { if (!b.used) cpu.fault("free of a block already freed"); b.used = false; return; }
    cpu.fault("free of a pointer malloc did not return");
}

// ---- printf ----------------------------------------------------------------------
std::string Runtime::format(Cpu &cpu, const std::string &fmt, Args &args) {
    std::string out;
    for (size_t i = 0; i < fmt.size(); i++) {
        if (fmt[i] != '%') { out += fmt[i]; continue; }
        std::string spec = "%";
        i++;
        while (i < fmt.size() && std::strchr("-+ #0", fmt[i])) spec += fmt[i++];
        if (i < fmt.size() && fmt[i] == '*') { spec += std::to_string(static_cast<int32_t>(args.word())); i++; }
        while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) spec += fmt[i++];
        if (i < fmt.size() && fmt[i] == '.') {
            spec += fmt[i++];
            if (i < fmt.size() && fmt[i] == '*') { spec += std::to_string(static_cast<int32_t>(args.word())); i++; }
            while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) spec += fmt[i++];
        }
        int longs = 0;
        bool shortInt = false, charInt = false;
        while (i < fmt.size() && std::strchr("hlLqjzt", fmt[i])) {
            if (fmt[i] == 'l' || fmt[i] == 'q' || fmt[i] == 'j') longs++;
            else if (fmt[i] == 'h') { if (shortInt) charInt = true; shortInt = true; }
            i++;
        }
        if (i >= fmt.size()) break;
        char conv = fmt[i];
        char buf[512];
        switch (conv) {
        case 'd': case 'i': {
            long long v;
            if (longs >= 2) v = static_cast<long long>(args.wide());
            else { int32_t w = static_cast<int32_t>(args.word()); v = charInt ? static_cast<signed char>(w) : shortInt ? static_cast<short>(w) : w; }
            std::snprintf(buf, sizeof buf, (spec + "lld").c_str(), v);
            break;
        }
        case 'u': case 'x': case 'X': case 'o': {
            unsigned long long v;
            if (longs >= 2) v = args.wide();
            else { uint32_t w = args.word(); v = charInt ? static_cast<unsigned char>(w) : shortInt ? static_cast<unsigned short>(w) : w; }
            std::snprintf(buf, sizeof buf, (spec + "ll" + conv).c_str(), v);
            break;
        }
        case 'c': std::snprintf(buf, sizeof buf, spec.append("c").c_str(), static_cast<int>(args.word())); break;
        case 's': {
            uint32_t p = args.word();
            std::string s = p == 0 ? "(null)" : cpu.readString(p);
            std::snprintf(buf, sizeof buf, spec.append("s").c_str(), s.c_str());
            break;
        }
        case 'p': std::snprintf(buf, sizeof buf, "0x%x", args.word()); break;
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
            // A NaN or an infinity is spelt here, not by the host, whose
            // spelling differs (Windows writes -nan(ind)); the program's
            // output must not depend on where the emulator runs.
            double v = args.dbl();
            if (v != v || std::isinf(v)) {
                std::string word = v != v ? "nan" : "inf";
                if (std::isupper(static_cast<unsigned char>(conv))) for (char &ch : word) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                if (std::signbit(v)) word = "-" + word;
                else if (spec.find('+') != std::string::npos) word = "+" + word;
                else if (spec.find(' ') != std::string::npos) word = " " + word;
                std::string w = spec;                        // keep the width, drop 0 and the precision
                std::string flags, width;
                size_t k = 1;
                while (k < w.size() && std::strchr("-+ #0", w[k])) { if (w[k] == '-') flags += '-'; k++; }
                while (k < w.size() && std::isdigit(static_cast<unsigned char>(w[k]))) width += w[k++];
                std::snprintf(buf, sizeof buf, ("%" + flags + width + "s").c_str(), word.c_str());
                break;
            }
            std::snprintf(buf, sizeof buf, (spec + conv).c_str(), v);
            break;
        }
        case 'n': { uint32_t p = args.word(); cpu.store32(p, static_cast<uint32_t>(out.size())); buf[0] = 0; break; }
        case '%': std::strcpy(buf, "%"); break;
        default: std::snprintf(buf, sizeof buf, "%s%c", spec.c_str(), conv); break;
        }
        out += buf;
    }
    return out;
}

// ---- what the runtime contributes as data ----------------------------------------
// The streams as objects; the Itanium ABI's three typeinfo classes' vtables,
// which a program's typeinfo objects point at (their address point, +8) and
// __dynamic_cast tells apart by the word there (1: no bases, 2: one base at
// offset 0, 3: several); std::type_info's own; and __dso_handle.
std::string Runtime::prelude() {
    return
        "\t.data\n"
        "\t.global stdin\n\t.global stdout\n\t.global stderr\n"
        "stdin:\t.word 1\nstdout:\t.word 2\nstderr:\t.word 3\n"
        "\t.global __dso_handle\n__dso_handle:\t.word 0\n"
        "\t.global _ZTVN10__cxxabiv117__class_type_infoE\n"
        "_ZTVN10__cxxabiv117__class_type_infoE:\t.word 0, 0, 1, 0, 0, 0, 0, 0\n"
        "\t.global _ZTVN10__cxxabiv120__si_class_type_infoE\n"
        "_ZTVN10__cxxabiv120__si_class_type_infoE:\t.word 0, 0, 2, 0, 0, 0, 0, 0\n"
        "\t.global _ZTVN10__cxxabiv121__vmi_class_type_infoE\n"
        "_ZTVN10__cxxabiv121__vmi_class_type_infoE:\t.word 0, 0, 3, 0, 0, 0, 0, 0\n"
        "\t.global _ZTVSt9type_info\n"
        "_ZTVSt9type_info:\t.word 0, 0, 0, 0, 0, 0, 0, 0\n";
}

void Runtime::runAtExit(Cpu &cpu) {
    cpu.resume();
    for (size_t i = atExit_.size(); i-- > 0; ) cpu.callback(atExit_[i].fn, atExit_[i].arg, 0);
    atExit_.clear();
}

void Runtime::terminate(Cpu &cpu, const char *why) {
    std::fflush(stdout);
    std::fprintf(stderr, "vm6747: terminate called: %s\n", why);
    cpu.exitWith(134);
    // exitWith stops the run; nothing after a terminate may continue.
    std::exit(134);
}

// ---- __dynamic_cast ------------------------------------------------------------
// The subobjects of the complete object, walked from its typeinfo: a class
// typeinfo is [vptr][name]; si adds [base]; vmi adds [flags][count] then
// [base][offset<<8 | flags] per base, a virtual base's offset naming the
// vbase_offset slot in the vtable of the subobject that holds it.
namespace {
struct Sub { uint32_t ti, addr; bool pub; };
void walk(Cpu &c, uint32_t ti, uint32_t addr, bool pub, std::vector<Sub> &out, int depth) {
    if (depth > 64) return;
    Sub s; s.ti = ti; s.addr = addr; s.pub = pub;
    out.push_back(s);
    uint32_t kind = c.load32(c.load32(ti));
    if (kind == 2) { walk(c, c.load32(ti + 8), addr, pub, out, depth + 1); return; }
    if (kind != 3) return;
    uint32_t count = c.load32(ti + 12);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t bti = c.load32(ti + 16 + 8 * i);
        int32_t of = static_cast<int32_t>(c.load32(ti + 20 + 8 * i));
        int32_t off = of >> 8;
        bool basePub = (of & 2) != 0, virt = (of & 1) != 0;
        uint32_t baseAddr;
        if (virt) { uint32_t vptr = c.load32(addr); baseAddr = addr + static_cast<int32_t>(c.load32(vptr + off)); }
        else baseAddr = addr + off;
        walk(c, bti, baseAddr, pub && basePub, out, depth + 1);
    }
}
bool sameType(Cpu &c, uint32_t a, uint32_t b) {
    if (a == b) return true;
    return c.readString(c.load32(a + 4)) == c.readString(c.load32(b + 4));
}
}

uint32_t Runtime::dynamicCast(Cpu &c, uint32_t sub, uint32_t src, uint32_t dst) {
    if (sub == 0) return 0;
    uint32_t vptr = c.load32(sub);
    uint32_t complete = sub + static_cast<int32_t>(c.load32(vptr - 8));
    uint32_t cti = c.load32(vptr - 4);
    std::vector<Sub> subs;
    walk(c, cti, complete, true, subs, 0);
    // The source must be a public base of the complete object at `sub`.
    bool srcPublic = false;
    for (const Sub &s : subs) if (s.addr == sub && sameType(c, s.ti, src) && s.pub) srcPublic = true;
    if (!srcPublic) return 0;
    if (sameType(c, cti, dst)) return complete;
    // Otherwise the unique public dst subobject; two of them is ambiguous.
    uint32_t found = 0; int n = 0;
    for (const Sub &s : subs) if (s.pub && sameType(c, s.ti, dst)) { if (n == 0 || s.addr != found) { found = s.addr; n++; } }
    return n == 1 ? found : 0;
}

// ---- the library -----------------------------------------------------------------
std::vector<std::string> Runtime::names() {
    static const char *const k[] = {
        "putchar", "puts", "printf", "vprintf", "sprintf", "vsprintf", "snprintf", "vsnprintf",
        "fprintf", "vfprintf", "fputs", "fputc", "putc", "fwrite", "fflush", "fopen", "fclose",
        "fgetc", "getc", "getchar", "fgets", "fread", "ftell", "fseek", "rewind", "feof", "remove",
        "perror", "ferror", "clearerr", "__errno_location", "__assert_fail", "__assert_rtn", "_assert",
        "exit", "abort", "atexit", "malloc", "calloc", "realloc", "free",
        "memcpy", "memmove", "memset", "memcmp", "memchr",
        "strlen", "strcpy", "strncpy", "strcat", "strncat", "strcmp", "strncmp", "strchr", "strrchr",
        "strstr", "strpbrk", "strspn", "strcspn", "strtok", "strerror", "strdup",
        "atoi", "atol", "atof", "strtol", "strtoul", "strtod", "abs", "labs", "llabs", "rand", "srand",
        "qsort", "bsearch",
        "isalnum", "isalpha", "iscntrl", "isdigit", "isgraph", "islower", "isprint", "ispunct",
        "isspace", "isupper", "isxdigit", "tolower", "toupper",
        "sqrt", "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "sinh", "cosh", "tanh",
        "exp", "log", "log10", "pow", "fabs", "floor", "ceil", "fmod", "ldexp", "frexp", "modf",
        "sqrtf", "fabsf", "floorf", "ceilf",
        "setjmp", "_setjmp", "longjmp", "time", "clock",
        "signal", "raise", "mktime", "localtime", "gmtime", "strftime", "difftime",
        "setlocale", "localeconv",
        // the C++ ABI
        "_Znwm", "_Znam", "_Znwj", "_Znaj", "_ZdlPv", "_ZdaPv", "_ZdlPvm", "_ZdaPvm", "_ZdlPvj", "_ZdaPvj",
        "__cxa_guard_acquire", "__cxa_guard_release", "__cxa_guard_abort", "__cxa_atexit",
        "__dynamic_cast", "__cxa_pure_virtual", "__cxa_deleted_virtual",
        "__cxa_allocate_exception", "__cxa_free_exception", "__cxa_throw", "__cxa_rethrow",
        "__cxa_begin_catch", "__cxa_end_catch", "__cxa_get_exception_ptr", "_Unwind_Resume",
        "__gxx_personality_v0", "__cxa_bad_cast", "__cxa_bad_typeid", "_ZSt9terminatev",
        "_ZNKSt9type_infoeqERKS_", "_ZNKSt9type_infoneERKS_", "_ZNKSt9type_info4nameEv",
        "_ZNKSt9type_info6beforeERKS_",
        "__c6xabi_divi", "__c6xabi_divu", "__c6xabi_remi", "__c6xabi_remu",
        "__c6xabi_divlli", "__c6xabi_divull", "__c6xabi_remlli", "__c6xabi_remull",
        "__c6xabi_divf", "__c6xabi_divd", "__c6xabi_fixfu", "__c6xabi_fixdu",
        "__c6xabi_fixflli", "__c6xabi_fixdlli", "__c6xabi_fixfull", "__c6xabi_fixdull",
        "__c6xabi_fltllif", "__c6xabi_fltllid", "__c6xabi_fltullf", "__c6xabi_fltulld",
    };
    return std::vector<std::string>(k, k + sizeof k / sizeof k[0]);
}

bool Runtime::call(const std::string &n, Cpu &c) {
    // ---- output ----
    if (n == "putchar") { std::putchar(static_cast<int>(arg(c, 0))); ret(c, arg(c, 0) & 0xff); return true; }
    if (n == "puts") { std::fputs(c.readString(arg(c, 0)).c_str(), stdout); std::putchar('\n'); ret(c, 1); return true; }
    if (n == "printf") {
        Args a = { c, variadicAt(c, 1) };
        std::string fmt = c.readString(a.word());
        std::string s = format(c, fmt, a);
        std::fwrite(s.data(), 1, s.size(), stdout);
        ret(c, static_cast<uint32_t>(s.size()));
        return true;
    }
    if (n == "vprintf") {
        Args a = { c, arg(c, 1) };
        std::string s = format(c, c.readString(arg(c, 0)), a);
        std::fwrite(s.data(), 1, s.size(), stdout);
        ret(c, static_cast<uint32_t>(s.size()));
        return true;
    }
    if (n == "sprintf" || n == "snprintf" || n == "vsprintf" || n == "vsnprintf") {
        uint32_t buf = arg(c, 0);
        bool sized = n == "snprintf" || n == "vsnprintf";
        uint32_t cap = sized ? arg(c, 1) : 0xffffffffu;
        int fmtIx = sized ? 2 : 1;
        Args a = n[0] == 'v' ? Args{ c, arg(c, fmtIx + 1) } : Args{ c, variadicAt(c, fmtIx + 1) };
        std::string fmt = c.readString(n[0] == 'v' ? arg(c, fmtIx) : a.word());
        std::string s = format(c, fmt, a);
        if (cap > 0) {
            uint32_t k = static_cast<uint32_t>(s.size()) < cap - 1 ? static_cast<uint32_t>(s.size()) : cap - 1;
            c.writeBytes(buf, s.data(), k);
            c.store8(buf + k, 0);
        }
        ret(c, static_cast<uint32_t>(s.size()));
        return true;
    }
    if (n == "fprintf" || n == "vfprintf") {
        uint32_t stream = arg(c, 0);
        Args a = n[0] == 'v' ? Args{ c, arg(c, 2) } : Args{ c, variadicAt(c, 2) };
        std::string fmt = c.readString(n[0] == 'v' ? arg(c, 1) : a.word());
        std::string s = format(c, fmt, a);
        FILE *f = stream == 2 ? stdout : stream == 3 ? stderr : nullptr;
        if (f == nullptr && stream >= 4 && stream - 4 < files_.size()) { files_[stream - 4].data += s; }
        else if (f != nullptr) std::fwrite(s.data(), 1, s.size(), f);
        ret(c, static_cast<uint32_t>(s.size()));
        return true;
    }
    if (n == "fputs" || n == "fputc" || n == "putc" || n == "fwrite") {
        uint32_t stream = n == "fwrite" ? arg(c, 3) : arg(c, 1);
        std::string s;
        if (n == "fputs") s = c.readString(arg(c, 0));
        else if (n == "fwrite") { uint32_t k = arg(c, 1) * arg(c, 2); for (uint32_t i = 0; i < k; i++) s += static_cast<char>(c.load8(arg(c, 0) + i)); }
        else s = std::string(1, static_cast<char>(arg(c, 0)));
        if (stream == 2) std::fwrite(s.data(), 1, s.size(), stdout);
        else if (stream == 3) std::fwrite(s.data(), 1, s.size(), stderr);
        else if (stream >= 4 && stream - 4 < files_.size()) files_[stream - 4].data += s;
        ret(c, n == "fwrite" ? arg(c, 2) : n == "fputs" ? 1 : (arg(c, 0) & 0xff));
        return true;
    }
    if (n == "fflush") { std::fflush(stdout); ret(c, 0); return true; }
    if (n == "ferror" || n == "clearerr") { ret(c, 0); return true; }
    if (n == "__assert_fail" || n == "__assert_rtn" || n == "_assert") {
        // glibc's (expr, file, line, function), Darwin's (function, file, line, expr), UCRT's (expr, file, line)
        std::string expr = c.readString(arg(c, n == "__assert_rtn" ? 3 : 0)), file = c.readString(arg(c, 1));
        std::fflush(stdout);
        std::fprintf(stderr, "Assertion failed: %s, file %s, line %u\n", expr.c_str(), file.c_str(), arg(c, 2));
        c.exitWith(134);
        return true;
    }
    if (n == "__errno_location") { ret(c, errnoAt(c)); return true; }
    if (n == "perror") { std::string s = c.readString(arg(c, 0)); std::fprintf(stderr, "%s: error\n", s.c_str()); return true; }
    // ---- files: kept in memory on the host until closed ----
    if (n == "fopen") {
        std::string path = c.readString(arg(c, 0)), mode = c.readString(arg(c, 1));
#ifdef _WIN32
        // A program that writes to /tmp means the scratch directory; give it
        // this host's, so the same corpus runs here.
        if (path.compare(0, 5, "/tmp/") == 0) {
            const char *t = std::getenv("TEMP");
            path = std::string(t != nullptr ? t : ".") + "\\" + path.substr(5);
        }
#endif
        File f; f.path = path; f.pos = 0; f.write = mode.find('w') != std::string::npos || mode.find('a') != std::string::npos;
        if (!f.write || mode.find('+') != std::string::npos || mode[0] == 'a') {
            FILE *h = std::fopen(path.c_str(), "rb");
            if (h == nullptr && !f.write) { setErrno(c, 2); ret(c, 0); return true; }   // ENOENT
            if (h != nullptr) { char b[4096]; size_t k; while ((k = std::fread(b, 1, sizeof b, h)) > 0) f.data.append(b, k); std::fclose(h); }
            if (mode[0] == 'w') f.data.clear();
            if (mode[0] == 'a') f.pos = f.data.size();
        }
        files_.push_back(f);
        ret(c, 4 + static_cast<uint32_t>(files_.size()) - 1);
        return true;
    }
    if (n == "fclose") {
        uint32_t s = arg(c, 0);
        if (s >= 4 && s - 4 < files_.size()) {
            File &f = files_[s - 4];
            if (f.write) { FILE *h = std::fopen(f.path.c_str(), "wb"); if (h) { std::fwrite(f.data.data(), 1, f.data.size(), h); std::fclose(h); } }
        }
        ret(c, 0);
        return true;
    }
    if (n == "fgetc" || n == "getc" || n == "getchar") {
        uint32_t s = n == "getchar" ? 1 : arg(c, 0);
        int ch = -1;
        if (s == 1) ch = std::getchar();
        else if (s >= 4 && s - 4 < files_.size()) { File &f = files_[s - 4]; if (f.pos < f.data.size()) ch = static_cast<unsigned char>(f.data[f.pos++]); }
        ret(c, static_cast<uint32_t>(ch));
        return true;
    }
    if (n == "fgets") {
        uint32_t buf = arg(c, 0), cap = arg(c, 1), s = arg(c, 2);
        std::string line;
        bool any = false;
        while (line.size() + 1 < cap) {
            int ch = -1;
            if (s == 1) ch = std::getchar();
            else if (s >= 4 && s - 4 < files_.size()) { File &f = files_[s - 4]; if (f.pos < f.data.size()) ch = static_cast<unsigned char>(f.data[f.pos++]); }
            if (ch < 0) break;
            any = true; line += static_cast<char>(ch);
            if (ch == '\n') break;
        }
        if (!any) { ret(c, 0); return true; }
        c.writeBytes(buf, line.data(), static_cast<uint32_t>(line.size()));
        c.store8(buf + static_cast<uint32_t>(line.size()), 0);
        ret(c, buf);
        return true;
    }
    if (n == "fread") {
        uint32_t buf = arg(c, 0), k = arg(c, 1) * arg(c, 2), s = arg(c, 3);
        uint32_t got = 0;
        if (s >= 4 && s - 4 < files_.size()) { File &f = files_[s - 4]; while (got < k && f.pos < f.data.size()) c.store8(buf + got++, static_cast<uint8_t>(f.data[f.pos++])); }
        ret(c, arg(c, 1) ? got / arg(c, 1) : 0);
        return true;
    }
    if (n == "ftell") { uint32_t s = arg(c, 0); ret(c, s >= 4 && s - 4 < files_.size() ? static_cast<uint32_t>(files_[s - 4].write ? files_[s - 4].data.size() : files_[s - 4].pos) : 0); return true; }
    if (n == "fseek") {
        uint32_t s = arg(c, 0); int32_t off = static_cast<int32_t>(arg(c, 1)); uint32_t whence = arg(c, 2);
        if (s >= 4 && s - 4 < files_.size()) { File &f = files_[s - 4]; size_t base = whence == 0 ? 0 : whence == 1 ? f.pos : f.data.size(); f.pos = base + off; }
        ret(c, 0);
        return true;
    }
    if (n == "rewind") { uint32_t s = arg(c, 0); if (s >= 4 && s - 4 < files_.size()) files_[s - 4].pos = 0; return true; }
    if (n == "feof") { uint32_t s = arg(c, 0); ret(c, s >= 4 && s - 4 < files_.size() ? files_[s - 4].pos >= files_[s - 4].data.size() : 0); return true; }
    if (n == "remove") { ret(c, std::remove(c.readString(arg(c, 0)).c_str()) == 0 ? 0 : 0xffffffffu); return true; }
    // ---- process ----
    if (n == "exit") { std::fflush(stdout); c.exitWith(static_cast<int>(arg(c, 0))); return true; }
    if (n == "abort") { std::fflush(stdout); std::fprintf(stderr, "vm6747: abort() called\n"); c.exitWith(134); return true; }
    if (n == "atexit") { AtExit a; a.fn = arg(c, 0); a.arg = 0; atExit_.push_back(a); ret(c, 0); return true; }
    if (n == "time") { uint32_t p = arg(c, 0); uint32_t t = static_cast<uint32_t>(std::time(nullptr)); if (p) c.store32(p, t); ret(c, t); return true; }
    // ---- signals: a table of handlers, raise calls one ----
    if (n == "signal") {
        uint32_t sig = arg(c, 0), handler = arg(c, 1);
        if (sig >= 32) { ret(c, 0xffffffffu); return true; }
        uint32_t prev = handlers_[sig];
        handlers_[sig] = handler;
        ret(c, prev);
        return true;
    }
    if (n == "raise") {
        uint32_t sig = arg(c, 0);
        uint32_t h = sig < 32 ? handlers_[sig] : 0;
        if (h == 1) { ret(c, 0); return true; }                       // SIG_IGN
        if (h == 0) { std::fflush(stdout); std::fprintf(stderr, "vm6747: signal %u\n", sig); c.exitWith(128 + static_cast<int>(sig)); return true; }
        handlers_[sig] = 0;                                           // reset, as SIGTERM's default does
        c.callback(h, sig, 0);
        ret(c, 0);
        return true;
    }
    // ---- time: struct tm is nine ints, then a long and a pointer here ----
    if (n == "mktime" || n == "strftime" || n == "localtime" || n == "gmtime" || n == "difftime") {
        if (n == "difftime") { retDouble(c, static_cast<double>(static_cast<int32_t>(arg(c, 0))) - static_cast<double>(static_cast<int32_t>(arg(c, 1)))); return true; }
        auto readTm = [&](uint32_t at, std::tm &t) {
            std::memset(&t, 0, sizeof t);
            int *f[9] = { &t.tm_sec, &t.tm_min, &t.tm_hour, &t.tm_mday, &t.tm_mon, &t.tm_year, &t.tm_wday, &t.tm_yday, &t.tm_isdst };
            for (int i = 0; i < 9; i++) *f[i] = static_cast<int32_t>(c.load32(at + 4 * i));
        };
        auto writeTm = [&](uint32_t at, const std::tm &t) {
            const int v[9] = { t.tm_sec, t.tm_min, t.tm_hour, t.tm_mday, t.tm_mon, t.tm_year, t.tm_wday, t.tm_yday, t.tm_isdst };
            for (int i = 0; i < 9; i++) c.store32(at + 4 * i, static_cast<uint32_t>(v[i]));
            c.store32(at + 36, 0); c.store32(at + 40, 0);
        };
        if (n == "mktime") { std::tm t; readTm(arg(c, 0), t); std::time_t r = std::mktime(&t); writeTm(arg(c, 0), t); ret(c, static_cast<uint32_t>(r)); return true; }
        if (n == "strftime") {
            std::tm t; readTm(arg(c, 3), t);
            std::string fmt = c.readString(arg(c, 2));
            char buf[512];
            size_t k = std::strftime(buf, sizeof buf, fmt.c_str(), &t);
            if (k + 1 > arg(c, 1)) { ret(c, 0); return true; }
            c.writeBytes(arg(c, 0), buf, static_cast<uint32_t>(k) + 1);
            ret(c, static_cast<uint32_t>(k));
            return true;
        }
        static uint32_t tmAt = 0;
        if (tmAt == 0) tmAt = allocate(c, 44);
        std::time_t tt = static_cast<int32_t>(c.load32(arg(c, 0)));
        std::tm *t = n == "gmtime" ? std::gmtime(&tt) : std::localtime(&tt);
        if (t == nullptr) { ret(c, 0); return true; }
        writeTm(tmAt, *t);
        ret(c, tmAt);
        return true;
    }
    // ---- locale: "C", and nothing else ----
    if (n == "setlocale") {
        static uint32_t name = 0;
        if (name == 0) { name = allocate(c, 8); c.writeBytes(name, "C\0", 2); }
        uint32_t want = arg(c, 1);
        if (want != 0) { std::string s = c.readString(want); if (!s.empty() && s != "C" && s != "POSIX") { ret(c, 0); return true; } }
        ret(c, name);
        return true;
    }
    if (n == "localeconv") {
        // Ten char * then eight char, as <locale.h> lays struct lconv out.
        static uint32_t lc = 0;
        if (lc == 0) {
            lc = allocate(c, 48);
            uint32_t dot = allocate(c, 8); c.writeBytes(dot, ".\0", 2);
            uint32_t empty = allocate(c, 8); c.store8(empty, 0);
            c.store32(lc, dot);
            for (int i = 1; i < 10; i++) c.store32(lc + 4 * i, empty);
            for (int i = 0; i < 8; i++) c.store8(lc + 40 + i, 127);   // CHAR_MAX: not available
        }
        ret(c, lc);
        return true;
    }
    if (n == "clock") { ret(c, static_cast<uint32_t>(c.cycle() / 1000)); return true; }
    // ---- memory ----
    if (n == "malloc") { ret(c, allocate(c, arg(c, 0))); return true; }
    if (n == "calloc") { uint32_t k = arg(c, 0) * arg(c, 1); uint32_t p = allocate(c, k); for (uint32_t i = 0; i < k; i++) c.store8(p + i, 0); ret(c, p); return true; }
    if (n == "realloc") {
        uint32_t old = arg(c, 0), size = arg(c, 1);
        uint32_t p = allocate(c, size);
        if (old != 0) {
            uint32_t oldSize = 0;
            for (const Block &b : blocks_) if (b.at == old) oldSize = b.size;
            for (uint32_t i = 0; i < oldSize && i < size; i++) c.store8(p + i, c.load8(old + i));
            release(c, old);
        }
        ret(c, p);
        return true;
    }
    if (n == "free") { release(c, arg(c, 0)); return true; }
    if (n == "memcpy" || n == "memmove") {
        uint32_t d = arg(c, 0), s = arg(c, 1), k = arg(c, 2);
        std::string tmp; for (uint32_t i = 0; i < k; i++) tmp += static_cast<char>(c.load8(s + i));
        for (uint32_t i = 0; i < k; i++) c.store8(d + i, static_cast<uint8_t>(tmp[i]));
        ret(c, d); return true;
    }
    if (n == "memset") { uint32_t d = arg(c, 0); for (uint32_t i = 0; i < arg(c, 2); i++) c.store8(d + i, static_cast<uint8_t>(arg(c, 1))); ret(c, d); return true; }
    if (n == "memcmp") {
        uint32_t a = arg(c, 0), b = arg(c, 1), k = arg(c, 2);
        for (uint32_t i = 0; i < k; i++) { int x = c.load8(a + i), y = c.load8(b + i); if (x != y) { ret(c, static_cast<uint32_t>(x - y)); return true; } }
        ret(c, 0); return true;
    }
    if (n == "memchr") { uint32_t p = arg(c, 0); for (uint32_t i = 0; i < arg(c, 2); i++) if (c.load8(p + i) == (arg(c, 1) & 0xff)) { ret(c, p + i); return true; } ret(c, 0); return true; }
    // ---- strings ----
    if (n == "strlen") { ret(c, static_cast<uint32_t>(c.readString(arg(c, 0)).size())); return true; }
    if (n == "strcpy" || n == "strcat") {
        uint32_t d = arg(c, 0); std::string s = c.readString(arg(c, 1));
        uint32_t at = d + (n == "strcat" ? static_cast<uint32_t>(c.readString(d).size()) : 0);
        c.writeBytes(at, s.data(), static_cast<uint32_t>(s.size())); c.store8(at + static_cast<uint32_t>(s.size()), 0);
        ret(c, d); return true;
    }
    if (n == "strncpy") {
        uint32_t d = arg(c, 0); std::string s = c.readString(arg(c, 1)); uint32_t k = arg(c, 2);
        for (uint32_t i = 0; i < k; i++) c.store8(d + i, i < s.size() ? static_cast<uint8_t>(s[i]) : 0);
        ret(c, d); return true;
    }
    if (n == "strncat") {
        uint32_t d = arg(c, 0); std::string s = c.readString(arg(c, 1)); uint32_t k = arg(c, 2);
        if (s.size() > k) s.resize(k);
        uint32_t at = d + static_cast<uint32_t>(c.readString(d).size());
        c.writeBytes(at, s.data(), static_cast<uint32_t>(s.size())); c.store8(at + static_cast<uint32_t>(s.size()), 0);
        ret(c, d); return true;
    }
    if (n == "strcmp") { int r = std::strcmp(c.readString(arg(c, 0)).c_str(), c.readString(arg(c, 1)).c_str()); ret(c, static_cast<uint32_t>(r < 0 ? -1 : r > 0 ? 1 : 0)); return true; }
    if (n == "strncmp") { int r = std::strncmp(c.readString(arg(c, 0)).c_str(), c.readString(arg(c, 1)).c_str(), arg(c, 2)); ret(c, static_cast<uint32_t>(r < 0 ? -1 : r > 0 ? 1 : 0)); return true; }
    if (n == "strchr" || n == "strrchr") {
        uint32_t p = arg(c, 0); std::string s = c.readString(p); char ch = static_cast<char>(arg(c, 1));
        size_t k = n == "strchr" ? s.find(ch) : s.rfind(ch);
        if (ch == 0) k = s.size();
        ret(c, k == std::string::npos ? 0 : p + static_cast<uint32_t>(k)); return true;
    }
    if (n == "strstr") { uint32_t p = arg(c, 0); std::string s = c.readString(p); size_t k = s.find(c.readString(arg(c, 1))); ret(c, k == std::string::npos ? 0 : p + static_cast<uint32_t>(k)); return true; }
    if (n == "strpbrk") { uint32_t p = arg(c, 0); std::string s = c.readString(p); size_t k = s.find_first_of(c.readString(arg(c, 1))); ret(c, k == std::string::npos ? 0 : p + static_cast<uint32_t>(k)); return true; }
    if (n == "strspn") { ret(c, static_cast<uint32_t>(std::strspn(c.readString(arg(c, 0)).c_str(), c.readString(arg(c, 1)).c_str()))); return true; }
    if (n == "strcspn") { ret(c, static_cast<uint32_t>(std::strcspn(c.readString(arg(c, 0)).c_str(), c.readString(arg(c, 1)).c_str()))); return true; }
    if (n == "strtok") {
        static uint32_t saved = 0;
        uint32_t p = arg(c, 0) ? arg(c, 0) : saved;
        std::string delim = c.readString(arg(c, 1));
        if (p == 0) { ret(c, 0); return true; }
        while (c.load8(p) != 0 && delim.find(static_cast<char>(c.load8(p))) != std::string::npos) p++;
        if (c.load8(p) == 0) { saved = 0; ret(c, 0); return true; }
        uint32_t start = p;
        while (c.load8(p) != 0 && delim.find(static_cast<char>(c.load8(p))) == std::string::npos) p++;
        if (c.load8(p) != 0) { c.store8(p, 0); saved = p + 1; } else saved = 0;
        ret(c, start); return true;
    }
    if (n == "strerror") { static uint32_t msg = 0; if (msg == 0) { msg = allocate(c, 32); c.writeBytes(msg, "error\0", 6); } ret(c, msg); return true; }
    if (n == "strdup") { std::string s = c.readString(arg(c, 0)); uint32_t p = allocate(c, static_cast<uint32_t>(s.size()) + 1); c.writeBytes(p, s.c_str(), static_cast<uint32_t>(s.size()) + 1); ret(c, p); return true; }
    // ---- conversions ----
    if (n == "atoi" || n == "atol") { ret(c, static_cast<uint32_t>(std::atoi(c.readString(arg(c, 0)).c_str()))); return true; }
    if (n == "atof") { retDouble(c, std::atof(c.readString(arg(c, 0)).c_str())); return true; }
    if (n == "strtol" || n == "strtoul" || n == "strtod") {
        uint32_t p = arg(c, 0); std::string s = c.readString(p); uint32_t endp = arg(c, 1);
        char *end = nullptr;
        if (n == "strtod") retDouble(c, std::strtod(s.c_str(), &end));
        else if (n == "strtol") ret(c, static_cast<uint32_t>(std::strtol(s.c_str(), &end, static_cast<int>(arg(c, 2)))));
        else ret(c, static_cast<uint32_t>(std::strtoul(s.c_str(), &end, static_cast<int>(arg(c, 2)))));
        if (endp != 0) c.store32(endp, p + static_cast<uint32_t>(end - s.c_str()));
        return true;
    }
    if (n == "abs" || n == "labs") { int32_t v = static_cast<int32_t>(arg(c, 0)); ret(c, static_cast<uint32_t>(v < 0 ? -v : v)); return true; }
    if (n == "llabs") { int64_t v = static_cast<int64_t>(argWide(c, 0)); retWide(c, static_cast<uint64_t>(v < 0 ? -v : v)); return true; }
    if (n == "rand") { ret(c, static_cast<uint32_t>(std::rand())); return true; }
    if (n == "srand") { std::srand(arg(c, 0)); return true; }
    if (n == "qsort") {
        uint32_t base = arg(c, 0), count = arg(c, 1), size = arg(c, 2), cmp = arg(c, 3);
        // An insertion sort through the program's comparator: the order of
        // comparisons differs from the host's qsort, which a comparator with
        // side effects would show, and nothing else.
        std::string tmp(size, '\0');
        for (uint32_t i = 1; i < count; i++) {
            for (uint32_t j = i; j > 0; j--) {
                uint32_t a = base + (j - 1) * size, b = base + j * size;
                if (static_cast<int32_t>(c.callback(cmp, a, b)) <= 0) break;
                for (uint32_t k = 0; k < size; k++) { uint8_t x = c.load8(a + k); c.store8(a + k, c.load8(b + k)); c.store8(b + k, x); }
            }
        }
        return true;
    }
    if (n == "bsearch") {
        uint32_t key = arg(c, 0), base = arg(c, 1), count = arg(c, 2), size = arg(c, 3), cmp = arg(c, 4);
        uint32_t lo = 0, hi = count;
        while (lo < hi) {
            uint32_t mid = (lo + hi) / 2;
            int32_t r = static_cast<int32_t>(c.callback(cmp, key, base + mid * size));
            if (r == 0) { ret(c, base + mid * size); return true; }
            if (r < 0) hi = mid; else lo = mid + 1;
        }
        ret(c, 0); return true;
    }
    // ---- ctype ----
    {
        static const char *const ct[] = { "isalnum", "isalpha", "iscntrl", "isdigit", "isgraph", "islower", "isprint", "ispunct", "isspace", "isupper", "isxdigit" };
        for (const char *name : ct) if (n == name) {
            int ch = static_cast<int>(arg(c, 0)); int r;
            if (n == "isalnum") r = std::isalnum(ch); else if (n == "isalpha") r = std::isalpha(ch); else if (n == "iscntrl") r = std::iscntrl(ch);
            else if (n == "isdigit") r = std::isdigit(ch); else if (n == "isgraph") r = std::isgraph(ch); else if (n == "islower") r = std::islower(ch);
            else if (n == "isprint") r = std::isprint(ch); else if (n == "ispunct") r = std::ispunct(ch); else if (n == "isspace") r = std::isspace(ch);
            else if (n == "isupper") r = std::isupper(ch); else r = std::isxdigit(ch);
            ret(c, r != 0); return true;
        }
    }
    if (n == "tolower") { ret(c, static_cast<uint32_t>(std::tolower(static_cast<int>(arg(c, 0))))); return true; }
    if (n == "toupper") { ret(c, static_cast<uint32_t>(std::toupper(static_cast<int>(arg(c, 0))))); return true; }
    // ---- math ----
    {
        struct M1 { const char *name; double (*f)(double); };
        static const M1 m1[] = { { "sqrt", std::sqrt }, { "sin", std::sin }, { "cos", std::cos }, { "tan", std::tan },
            { "asin", std::asin }, { "acos", std::acos }, { "atan", std::atan }, { "sinh", std::sinh }, { "cosh", std::cosh },
            { "tanh", std::tanh }, { "exp", std::exp }, { "log", std::log }, { "log10", std::log10 }, { "fabs", std::fabs },
            { "floor", std::floor }, { "ceil", std::ceil } };
        for (const M1 &m : m1) if (n == m.name) { retDouble(c, m.f(argDouble(c, 0))); return true; }
        struct M2 { const char *name; double (*f)(double, double); };
        static const M2 m2[] = { { "atan2", std::atan2 }, { "pow", std::pow }, { "fmod", std::fmod } };
        for (const M2 &m : m2) if (n == m.name) { retDouble(c, m.f(argDouble(c, 0), argDouble(c, 1))); return true; }
        if (n == "ldexp") { retDouble(c, std::ldexp(argDouble(c, 0), static_cast<int>(arg(c, 1)))); return true; }
        if (n == "frexp") { int e = 0; double r = std::frexp(argDouble(c, 0), &e); c.store32(arg(c, 1), static_cast<uint32_t>(e)); retDouble(c, r); return true; }
        if (n == "modf") { double ip = 0; double r = std::modf(argDouble(c, 0), &ip); uint64_t v; std::memcpy(&v, &ip, 8); c.store64(arg(c, 1), v); retDouble(c, r); return true; }
        if (n == "sqrtf") { retFloat(c, std::sqrt(argFloat(c, 0))); return true; }
        if (n == "fabsf") { retFloat(c, std::fabs(argFloat(c, 0))); return true; }
        if (n == "floorf") { retFloat(c, std::floor(argFloat(c, 0))); return true; }
        if (n == "ceilf") { retFloat(c, std::ceil(argFloat(c, 0))); return true; }
    }
    // ---- setjmp / longjmp: the callee-saved registers, the stack and the return ----
    if (n == "setjmp" || n == "_setjmp") {
        uint32_t env = arg(c, 0);
        static const int saved[] = { 10, 11, 12, 13, 14, 15, 16 + 10, 16 + 11, 16 + 12, 16 + 13, 16 + 14, 16 + 15, 16 + 3 };
        for (int i = 0; i < 13; i++) c.store32(env + 4 * i, c.reg(saved[i]));
        ret(c, 0);
        return true;
    }
    if (n == "longjmp") {
        uint32_t env = arg(c, 0), val = arg(c, 1);
        static const int saved[] = { 10, 11, 12, 13, 14, 15, 16 + 10, 16 + 11, 16 + 12, 16 + 13, 16 + 14, 16 + 15, 16 + 3 };
        for (int i = 0; i < 13; i++) c.setReg(saved[i], c.load32(env + 4 * i));
        ret(c, val == 0 ? 1 : val);           // and B3 is now setjmp's return address
        return true;
    }
    // ---- the C++ ABI ----
    if (n == "_Znwm" || n == "_Znam" || n == "_Znwj" || n == "_Znaj") { ret(c, allocate(c, arg(c, 0))); return true; }
    if (n == "_ZdlPv" || n == "_ZdaPv" || n == "_ZdlPvm" || n == "_ZdaPvm" || n == "_ZdlPvj" || n == "_ZdaPvj") { release(c, arg(c, 0)); return true; }
    if (n == "__cxa_guard_acquire") { ret(c, c.load8(arg(c, 0)) == 0); return true; }
    if (n == "__cxa_guard_release") { c.store8(arg(c, 0), 1); return true; }
    if (n == "__cxa_guard_abort") { return true; }
    if (n == "__cxa_atexit") { AtExit a; a.fn = arg(c, 0); a.arg = arg(c, 1); atExit_.push_back(a); ret(c, 0); return true; }
    if (n == "__dynamic_cast") { ret(c, dynamicCast(c, arg(c, 0), arg(c, 1), arg(c, 2))); return true; }
    if (n == "__cxa_pure_virtual") c.fault("a pure virtual function was called");
    if (n == "__cxa_deleted_virtual") c.fault("a deleted virtual function was called");
    if (n == "__cxa_allocate_exception") { ret(c, allocate(c, arg(c, 0) + 32)); return true; }
    if (n == "__cxa_free_exception") { release(c, arg(c, 0)); return true; }
    if (n == "__cxa_throw" || n == "__cxa_rethrow") terminate(c, "an exception was thrown, and this target does not unwind yet");
    if (n == "__cxa_bad_cast") terminate(c, "bad dynamic_cast to a reference");
    if (n == "__cxa_bad_typeid") terminate(c, "typeid of a null pointer");
    if (n == "_ZSt9terminatev") terminate(c, "std::terminate()");
    if (n == "__cxa_begin_catch" || n == "__cxa_end_catch" || n == "__cxa_get_exception_ptr" ||
        n == "_Unwind_Resume" || n == "__gxx_personality_v0")
        c.fault("'" + n + "' reached: no exception can be in flight on this target");
    if (n == "_ZNKSt9type_infoeqERKS_") { ret(c, sameType(c, arg(c, 0), arg(c, 1))); return true; }
    if (n == "_ZNKSt9type_infoneERKS_") { ret(c, !sameType(c, arg(c, 0), arg(c, 1))); return true; }
    if (n == "_ZNKSt9type_info4nameEv") { ret(c, c.load32(arg(c, 0) + 4)); return true; }
    if (n == "_ZNKSt9type_info6beforeERKS_") { ret(c, c.readString(c.load32(arg(c, 0) + 4)) < c.readString(c.load32(arg(c, 1) + 4))); return true; }
    // ---- the EABI helpers ----
    if (n == "__c6xabi_divi") { int32_t a = static_cast<int32_t>(c.reg(Cpu::A4)), b = static_cast<int32_t>(c.reg(Cpu::B4)); if (b == 0) c.fault("division by zero"); ret(c, static_cast<uint32_t>(b == -1 ? -a : a / b)); return true; }
    if (n == "__c6xabi_divu") { uint32_t a = c.reg(Cpu::A4), b = c.reg(Cpu::B4); if (b == 0) c.fault("division by zero"); ret(c, a / b); return true; }
    if (n == "__c6xabi_remi") { int32_t a = static_cast<int32_t>(c.reg(Cpu::A4)), b = static_cast<int32_t>(c.reg(Cpu::B4)); if (b == 0) c.fault("division by zero"); ret(c, static_cast<uint32_t>(b == -1 ? 0 : a % b)); return true; }
    if (n == "__c6xabi_remu") { uint32_t a = c.reg(Cpu::A4), b = c.reg(Cpu::B4); if (b == 0) c.fault("division by zero"); ret(c, a % b); return true; }
    if (n == "__c6xabi_divlli") { int64_t a = static_cast<int64_t>(c.pair(Cpu::A4)), b = static_cast<int64_t>(c.pair(Cpu::B4)); if (b == 0) c.fault("division by zero"); retWide(c, static_cast<uint64_t>(b == -1 ? -a : a / b)); return true; }
    if (n == "__c6xabi_divull") { uint64_t a = c.pair(Cpu::A4), b = c.pair(Cpu::B4); if (b == 0) c.fault("division by zero"); retWide(c, a / b); return true; }
    if (n == "__c6xabi_remlli") { int64_t a = static_cast<int64_t>(c.pair(Cpu::A4)), b = static_cast<int64_t>(c.pair(Cpu::B4)); if (b == 0) c.fault("division by zero"); retWide(c, static_cast<uint64_t>(b == -1 ? 0 : a % b)); return true; }
    if (n == "__c6xabi_remull") { uint64_t a = c.pair(Cpu::A4), b = c.pair(Cpu::B4); if (b == 0) c.fault("division by zero"); retWide(c, a % b); return true; }
    if (n == "__c6xabi_divf") { retFloat(c, argFloat(c, 0) / argFloat(c, 1)); return true; }
    if (n == "__c6xabi_divd") { retDouble(c, argDouble(c, 0) / argDouble(c, 1)); return true; }
    if (n == "__c6xabi_fixfu") { float f = argFloat(c, 0); ret(c, f != f || f <= 0 ? 0 : f >= 4294967295.0f ? 0xffffffffu : static_cast<uint32_t>(f)); return true; }
    if (n == "__c6xabi_fixdu") { double d = argDouble(c, 0); ret(c, d != d || d <= 0 ? 0 : d >= 4294967295.0 ? 0xffffffffu : static_cast<uint32_t>(d)); return true; }
    if (n == "__c6xabi_fixflli") { float f = argFloat(c, 0); retWide(c, static_cast<uint64_t>(f != f ? 0 : static_cast<int64_t>(f))); return true; }
    if (n == "__c6xabi_fixdlli") { double d = argDouble(c, 0); retWide(c, static_cast<uint64_t>(d != d ? 0 : static_cast<int64_t>(d))); return true; }
    if (n == "__c6xabi_fixfull") { float f = argFloat(c, 0); retWide(c, f != f || f <= 0 ? 0 : static_cast<uint64_t>(f)); return true; }
    if (n == "__c6xabi_fixdull") { double d = argDouble(c, 0); retWide(c, d != d || d <= 0 ? 0 : static_cast<uint64_t>(d)); return true; }
    if (n == "__c6xabi_fltllif") { retFloat(c, static_cast<float>(static_cast<int64_t>(argWide(c, 0)))); return true; }
    if (n == "__c6xabi_fltllid") { retDouble(c, static_cast<double>(static_cast<int64_t>(argWide(c, 0)))); return true; }
    if (n == "__c6xabi_fltullf") { retFloat(c, static_cast<float>(argWide(c, 0))); return true; }
    if (n == "__c6xabi_fltulld") { retDouble(c, static_cast<double>(argWide(c, 0))); return true; }
    return false;
}
