// A11 - a constexpr function call in a floating constant expression is refused
// cl:    3.50
// cpp11: error: 'H' is 'constexpr' ... this initialiser is not a constant expression
extern "C" int printf(const char *, ...);
constexpr double half(double d) { return d / 2.0; }
constexpr double H = half(7.0);
int main() { printf("%.2f\n", H); return 0; }
