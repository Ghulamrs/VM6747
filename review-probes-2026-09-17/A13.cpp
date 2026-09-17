// A13 - P g = P(); at file scope is refused
// cl:    0 0.0 5 3 0
// cxx1i: error: a struct or union at file scope needs a braced initialiser
extern "C" int printf(const char *, ...);
struct P { int a; double b; };
P g = P();
struct Q { int k; Q() : k(5) {} };
Q q = Q();
template <class T> struct Tm { static T st; };
template <class T> T Tm<T>::st = T();
int main() { Tm<P>::st.a = 3; int i = Tm<int>::st; printf("%d %.1f %d %d %d\n", g.a, g.b, q.k, Tm<P>::st.a, i); return 0; }
