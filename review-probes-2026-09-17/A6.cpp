// A6 - pointer-to-member widths under multiple and virtual inheritance (pm row), and sizeof of a class with two empty bases (class row, 7th)
// cl:    class 1 1 16 16 4 8 8 4 / pm 4 8 4 16 8 16
// cxx1i: class 1 1 16 16 4 8 4 4 / pm 4 8 4 8 4 8
extern "C" int printf(const char *, ...);
struct Empty {};
struct One { char c; };
struct WithV { virtual void f() {} int x; };
struct WithV2 { int x; virtual void f() {} char c; };
struct EB1 : Empty { int x; };
struct EB2 : Empty { Empty e; int x; };
struct Empty2 {};
struct EB3 : Empty, Empty2 { int x; };
struct Tail { int a; char b; Tail() : a(0), b(0) {} };
struct TailD : Tail { char c; };
struct TailPod { int a; char b; };
struct TailPodD : TailPod { char c; };
struct BF1 { char a : 4; int b : 4; };
struct BF2 { unsigned a : 3; unsigned b : 5; unsigned c : 25; };
struct BF3 { char a : 3; char b : 6; };
struct BF4 { long long a : 40; int b : 8; };
struct BF5 { bool a : 1; bool b : 1; int c : 1; };
struct BF6 { int a : 3; char z; int b : 3; };
struct BF7 { short a : 8; short b : 8; short c : 1; };
struct BF8 { unsigned a : 31; unsigned b : 2; };
struct BF9 { int : 0; char a; };
struct BF10 { char a; int : 0; char b; };
union U1 { char c; double d; };
union U2 { int a; char b[6]; };
enum E1 { E1a };
enum E2 { E2a = 0x7fffffff };
enum E3 { E3a = -1 };
struct V { int v; };
struct L : virtual V { int l; };
struct R : virtual V { int r; };
struct Dia : L, R { int d; };
struct LV : virtual V { virtual void g() {} int l; };
struct Arr { char a[3]; short s; };
struct Nest { char c; struct { int i; } n; };
struct Dbl { char c; double d; };
struct LD { char c; long double d; };
struct FP { void (*f)(); int (Empty::*pm)(); int Empty::*pd; };
struct Single { int x; int f(); };
struct Multi : One, Single { int y; };
struct Virt : virtual One { int z; };
int main() {
    printf("fund %d %d %d %d %d %d %d %d\n", (int)sizeof(bool), (int)sizeof(wchar_t), (int)sizeof(long), (int)sizeof(long long), (int)sizeof(long double), (int)sizeof(void *), (int)sizeof(E1), (int)sizeof(E3));
    printf("class %d %d %d %d %d %d %d %d\n", (int)sizeof(Empty), (int)sizeof(One), (int)sizeof(WithV), (int)sizeof(WithV2), (int)sizeof(EB1), (int)sizeof(EB2), (int)sizeof(EB3), (int)sizeof(E2));
    printf("tail %d %d %d %d\n", (int)sizeof(Tail), (int)sizeof(TailD), (int)sizeof(TailPod), (int)sizeof(TailPodD));
    printf("bf %d %d %d %d %d %d %d %d %d %d\n", (int)sizeof(BF1), (int)sizeof(BF2), (int)sizeof(BF3), (int)sizeof(BF4), (int)sizeof(BF5), (int)sizeof(BF6), (int)sizeof(BF7), (int)sizeof(BF8), (int)sizeof(BF9), (int)sizeof(BF10));
    printf("union %d %d\n", (int)sizeof(U1), (int)sizeof(U2));
    printf("vbase %d %d %d %d %d\n", (int)sizeof(V), (int)sizeof(L), (int)sizeof(R), (int)sizeof(Dia), (int)sizeof(LV));
    printf("misc %d %d %d %d %d\n", (int)sizeof(Arr), (int)sizeof(Nest), (int)sizeof(Dbl), (int)sizeof(LD), (int)sizeof(FP));
    printf("pm %d %d %d %d %d %d\n", (int)sizeof(int Single::*), (int)sizeof(int (Single::*)()), (int)sizeof(int Multi::*), (int)sizeof(int (Multi::*)()), (int)sizeof(int Virt::*), (int)sizeof(int (Virt::*)()));
    printf("align %d %d %d %d %d\n", (int)alignof(Dbl), (int)alignof(LD), (int)alignof(long double), (int)alignof(BF4), (int)alignof(WithV));
    return 0;
}
