// A17 - int ns::f(int) { } defining a namespace member by qualified name
// cl:    2 5
// cxx1i: error: 'ns' is not a class
extern "C" int printf(const char *, ...);
namespace ns { int f(int); int g(double); }
int ns::f(int x) { return x + 1; }
int ns::g(double d) { return (int)(d * 2); }
int main() { printf("%d %d\n", ns::f(1), ns::g(2.5)); return 0; }
