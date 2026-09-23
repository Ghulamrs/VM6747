// A24 - an override with a different return type is accepted and runs
// cl:    cl refuses: C2555
// cpp11: compiles; prints 0
extern "C" int printf(const char *, ...);
struct A { virtual int f() { return 1; } virtual ~A() {} };
struct B : A { double f() { return 2.5; } };
int main() { B b; A *p = &b; int r = p->f(); printf("%d\n", r); return 0; }
