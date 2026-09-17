// A20 - a pointer to a member of a nested class in a declarator
// cl:    2 5 3
// cxx1i: error: expected ','
extern "C" int printf(const char *, ...);
struct Nest { struct In { int v; int f(int k) { return k + v; } }; };
int call(int (Nest::In::*m)(int), Nest::In *o) { return (o->*m)(1); }
int main() { Nest::In i; i.v = 2; int Nest::In::*pd = &Nest::In::v; int (Nest::In::*pf)(int) = &Nest::In::f; printf("%d %d %d\n", i.*pd, (i.*pf)(3), call(pf, &i)); return 0; }
