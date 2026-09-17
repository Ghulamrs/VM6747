// F2 - arm64-darwin: std::string erase/clear/fill constructor crashes (bus error); same program is right on x86_64-windows and tms6747
// cl:    ac / [] 0 / qqq 3
// cxx1i: rc=138 (SIGBUS), no output; the sealed cxx1 the same
#include <string>
extern "C" int printf(const char *, ...);
int main() {
    std::string f = "abc"; f.erase(1, 1); printf("%s\n", f.c_str()); f.clear(); printf("[%s] %d\n", f.c_str(), (int)f.size());
    std::string g(3, 'q'); printf("%s %d\n", g.c_str(), (int)g.size());
    return 0;
}
