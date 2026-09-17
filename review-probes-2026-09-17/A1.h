// A1.h - shared header of the A1 pair
extern "C" int printf(const char *, ...);
struct NT { int x; NT(int v); NT(const NT &o); ~NT(); };
int takeNT(NT); NT makeNT(int);
