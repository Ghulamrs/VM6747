// A3.h - shared header of the A3 pair
extern "C" int printf(const char *, ...);
struct Virt { virtual ~Virt(); virtual int vf(int); virtual int g(int); virtual int g(double); virtual int h(); int base; Virt(); };
struct Plain2 { virtual int a(); virtual int b(); virtual int c(); virtual ~Plain2(); int p; Plain2(); };
int callVirt(Virt *v, int k); Virt *makeVirt(); int useVirt(Virt *v); int callPlain(Plain2 *p); Plain2 *makePlain();
