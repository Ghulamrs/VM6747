// A1 - callee half of the cross-link pair (A1-caller.cpp is the other half; A1.h the header): a cpp11-compiled constructor returns 0 in RAX, cl's caller uses RAX as this
// cl:    cl+cl: start / nt 14 / NT 14 / took 14
// cpp11: cpp11 callee + cl caller: start / nt 14 / rc=-1073741819 (access violation in takeNT)
#include "A1.h"
NT::NT(int v) : x(v) {} NT::NT(const NT &o) : x(o.x + 1000) {} NT::~NT() {}
int takeNT(NT n) { printf("NT %d\n", n.x % 1000); return n.x % 1000; }
NT makeNT(int v) { NT r(v); return r; }
