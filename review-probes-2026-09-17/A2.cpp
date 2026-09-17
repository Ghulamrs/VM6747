// A2 - a by-value parameter of a class with a destructor is destroyed twice by the callee when the function was declared before it was defined (x86_64-windows only)
// cl:    NT 14 / ~NT 14 / r=14 gone=1 / ~NT 14
// cxx1i: rc=-1073741819 and no output; -S shows two call ??1NT on the normal path of takeNT
extern "C" int printf(const char *, ...);
static int gone = 0;
struct NT { int x; NT(int v); NT(const NT &o); ~NT(); };
int takeNT(NT);
NT::NT(int v) : x(v) {} NT::NT(const NT &o) : x(o.x) {} NT::~NT() { ++gone; printf("~NT %d\n", x); }
int takeNT(NT n) { printf("NT %d\n", n.x); return n.x; }
int main() { NT a(14); int r = takeNT(a); printf("r=%d gone=%d\n", r, gone); return 0; }
