// A27 - dependent type used without typename
// cl:    cl refuses: C2061/C7510
// cpp11: compiles; prints 3
extern "C" int printf(const char *, ...);
struct T { typedef int type; };
template <class X> struct U { X::type v; };
int main() { U<T> u; u.v = 3; printf("%d\n", u.v); return 0; }
