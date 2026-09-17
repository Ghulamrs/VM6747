// A14 - template <class T> T Tm<T>::st; is a definition, read as a declaration
// cl:    4 5
// cxx1i: error: 'Tm::st' is declared here and not defined
extern "C" int printf(const char *, ...);
template <class T> struct Tm { static T st; };
template <class T> T Tm<T>::st;
struct P { int a; };
int main() { typedef Tm<int> TI; typedef Tm<P> TP; TI::st = 4; TP::st.a = 5; printf("%d %d\n", TI::st, TP::st.a); return 0; }
