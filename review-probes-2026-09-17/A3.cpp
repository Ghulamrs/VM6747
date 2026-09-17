// A3 - callee half of the cross-link pair (A3-caller.cpp, A3.h): overloaded virtuals g(int)/g(double) laid in declaration order where cl lays adjacent overloads in reverse
// cl:    callVirt 53 1003 2003 300 ... plain 729 1 2 3 101 201
// cxx1i: either mixed link: callVirt 53 2000 1003 300 ... plain 729 1 2 3 204 101
#include "A3.h"
Virt::Virt() : base(11) {} Virt::~Virt() { printf("~Virt %d\n", base); }
int Virt::vf(int k) { return k + base; } int Virt::g(int k) { return 100 + k; } int Virt::g(double d) { return 200 + (int)d; } int Virt::h() { return 300; }
Plain2::Plain2() : p(1) {} Plain2::~Plain2() { printf("~Plain2\n"); } int Plain2::a() { return 1; } int Plain2::b() { return 2; } int Plain2::c() { return 3; }
int callVirt(Virt *v, int k) { int a = v->vf(k); int b = v->g(k); int c = v->g(k + 0.5); int d = v->h(); printf("callVirt %d %d %d %d\n", a, b, c, d); return a + b + c + d; }
Virt *makeVirt() { return new Virt(); }
int useVirt(Virt *v) { int r = v->vf(1) + v->h(); delete v; return r; }
int callPlain(Plain2 *p) { int r = p->a() * 100 + p->b() * 10 + p->c(); printf("callPlain %d\n", r); return r; }
Plain2 *makePlain() { return new Plain2(); }
