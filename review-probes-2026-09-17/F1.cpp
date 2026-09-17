// F1 - Itanium targets: a local in a try body is not destroyed when a nested try's handler does not match (clang oracle; refused by name on x86_64-windows)
// cl:    +8 +9 -9 -8 outer 2 live=0 end live=0
// cxx1i: +8 +9 -9 outer 2 live=1 end live=1 (arm64-darwin, tms6747, and the sealed cxx1)
extern "C" int printf(const char *, ...);
static int live = 0;
struct R { int id; R(int i) : id(i) { ++live; printf("+%d\n", id); } ~R() { --live; printf("-%d\n", id); } };
int main() {
    try {
        R a(8);
        try { R b(9); throw 2; } catch (double) { printf("no\n"); }
    } catch (int v) { printf("outer %d live=%d\n", v, live); }
    printf("end live=%d\n", live);
    return 0;
}
