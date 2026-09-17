// A7 - Microsoft names: a global of pointer-to-member type is spelled ...A where cl spells ...EQ<class>@ (compile with -S and compare PUBLIC symbols)
// cl:    ?gpm@@3PEQNest@@HEQ1@  ?gpmf@@3P8Nest@@EAAHUIn@1@PEAU21@@ZEQ1@
// cxx1i: ?gpm@@3PEQNest@@HA  ?gpmf@@3P8Nest@@EAAHUIn@1@PEAU21@@ZA
namespace outer { namespace inner { struct Deep { int m; int get() const; static int s; }; int Deep::s = 1; int Deep::get() const { return m; } int free1(Deep, const Deep &, Deep *) { return 0; } } }
struct Nest { struct In { int v; int f(int); }; In in; int g(In, In *) { return 0; } };
int Nest::In::f(int k) { return k; }
template <class T> struct Tm { T v; T get() const { return v; } static T st; };
template <class T> T Tm<T>::st = 0;
template <class T, int N> struct Arr { T a[N]; int len() const { return N; } };
int useTm() { Tm<int> a; Tm<double> b; Arr<char, 4> d; a.v = 0; b.v = 0; typedef Tm<int> TI; TI::st = 1; return a.get() + (int)b.get() + d.len(); }
struct Op { int v; Op operator+(const Op &) const; Op &operator+=(int); bool operator==(const Op &) const; int operator[](int) const; int operator()(int, int); Op operator-() const; Op &operator++(); Op operator++(int); operator int() const; operator const char *() const; explicit Op(int); Op(); Op(const Op &); ~Op(); Op &operator=(const Op &); int *operator->(); };
Op Op::operator+(const Op &o) const { Op r(v + o.v); return r; } Op &Op::operator+=(int k) { v += k; return *this; } bool Op::operator==(const Op &o) const { return v == o.v; }
int Op::operator[](int i) const { return v * i; } int Op::operator()(int a, int b) { return v + a + b; } Op Op::operator-() const { Op r(-v); return r; } Op &Op::operator++() { ++v; return *this; } Op Op::operator++(int) { Op r(*this); ++v; return r; }
Op::operator int() const { return v; } Op::operator const char *() const { return 0; } Op::Op(int x) : v(x) {} Op::Op() : v(0) {} Op::Op(const Op &o) : v(o.v) {} Op::~Op() {} Op &Op::operator=(const Op &o) { v = o.v; return *this; } int *Op::operator->() { return &v; }
Op operator*(const Op &a, const Op &b) { Op r(a.v * b.v); return r; } bool operator<(const Op &a, const Op &b) { return a.v < b.v; }
enum Col { Red }; enum class_like { Q };
int s1(char, signed char, unsigned char, short, unsigned short, int, unsigned, long, unsigned long, long long, unsigned long long, float, double, long double, bool, wchar_t) { return 0; }
int s2(const char *, char *const, const char *const, char **, const char *&, char *&&, const int &, int &&) { return 0; }
int s3(int (*)(int), int (&)[3], int (*)[3], int Nest::*, int (Nest::*)(Nest::In, Nest::In *), void (*)(...), int (*)(int, ...)) { return 0; }
int s4(Col, Col *, const Col &, outer::inner::Deep, Tm<int>, Tm<double> *, Arr<char, 4> &) { return 0; }
int s5(int, int, int, char *, char *, const char *, const char *, Nest, Nest, Nest *, Nest *) { return 0; }
int s6(decltype(nullptr), unsigned char *, signed char *, const unsigned char *) { return 0; }
int s7(void) { return 0; } void s8(int, ...) {}
int s9(int (Nest::*)(Nest::In, Nest::In *), int Nest::*) { return 0; }
long long gll = 1; unsigned long gul = 2; const int gci = 3; const char *gcp = 0; char *const gcpc = 0; int garr[3]; Nest::In gin; Tm<int> gtm; double gd; bool gb; wchar_t gw; Col gcol; int *gp; const int *gpc; int (*gfp)(int); int Nest::*gpm; int (Nest::*gpmf)(Nest::In, Nest::In *);
static int sfun(int) { return 0; } int callsfun() { return sfun(1); }
extern "C" int cfun(int) { return 1; }
int main() { return useTm() + callsfun(); }
