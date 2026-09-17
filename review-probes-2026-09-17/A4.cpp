// A4 - two adjacent empty bases share offset 0; cl puts the second at 1
// cl:    1 2 8 8 8 8 12 8 8 / 1 4 4 4 4 8 4 4 / 8 4 8 4 16 8 / 0 1
// cxx1i: 1 1 4 4 8 8 8 8 8 / 0 0 0 4 4 4 4 4 / 8 4 4 0 8 0 / 0 0
extern "C" int printf(const char *, ...);
#define OFF(T, m) ((int)((char *)&((T *)64)->m - (char *)64))
struct E1 {}; struct E2 {}; struct E3 {};
struct NE { int n; };
struct A1 : E1, E2 {};
struct A2 : E1, E2 { char c; };
struct A3 : E1, E2 { int x; };
struct A4 : E1, E2, E3 { int x; };
struct A5 : NE, E1 { int x; };
struct A6 : E1, NE { int x; };
struct A7 : NE, E1, E2 { int x; };
struct A8 : E1 { E2 e; int x; };
struct A9 : E1, E2 { E3 e; int x; };
struct B1 : E1 { char c; }; struct B2 : B1, E2 { int x; };
struct C1 : E1 {}; struct C2 : C1, E2 { int x; };
struct D1 : E1 { double d; }; struct D2 : E1, E2 { double d; };
int main() {
    printf("%d %d %d %d %d %d %d %d %d\n", (int)sizeof(A1), (int)sizeof(A2), (int)sizeof(A3), (int)sizeof(A4), (int)sizeof(A5), (int)sizeof(A6), (int)sizeof(A7), (int)sizeof(A8), (int)sizeof(A9));
    printf("%d %d %d %d %d %d %d %d\n", OFF(A2, c), OFF(A3, x), OFF(A4, x), OFF(A5, x), OFF(A6, x), OFF(A7, x), OFF(A8, x), OFF(A9, x));
    printf("%d %d %d %d %d %d\n", (int)sizeof(B2), OFF(B2, x), (int)sizeof(C2), OFF(C2, x), (int)sizeof(D2), OFF(D2, d));
    A3 a3; E1 *pe1 = &a3; E2 *pe2 = &a3;
    printf("%d %d\n", (int)((char *)pe1 - (char *)&a3), (int)((char *)pe2 - (char *)&a3));
    return 0;
}
