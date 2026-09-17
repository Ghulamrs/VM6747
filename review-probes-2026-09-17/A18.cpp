// A18 - c.A::x qualified member access
// cl:    1 2 1
// cxx1i: error: 'struct C' has no member 'A'
extern "C" int printf(const char *, ...);
struct A { int x; int get() const { return x; } }; struct C : A { int y; };
int main() { C c; c.A::x = 1; c.y = 2; int g = c.A::get(); printf("%d %d %d\n", c.A::x, c.y, g); return 0; }
