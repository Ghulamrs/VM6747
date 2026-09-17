// A25 - ambiguous member of two bases picked silently
// cl:    cl refuses: C2385 ambiguous access of 'x'
// cxx1i: compiles; prints 1
extern "C" int printf(const char *, ...);
struct A { int x; }; struct B { int x; }; struct C : A, B {};
int main() { C c; c.x = 1; printf("%d\n", c.x); return 0; }
