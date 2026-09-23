// A21 - a lambda returning a lambda
// cl:    702
// cpp11: error: this function's return type is 'struct main::$deduced_0' and this is 'struct operator()::$_0'
extern "C" int printf(const char *, ...);
int main() { auto make = [](int base) { return [base](int x) { return base * 100 + x; }; }; auto m = make(7); printf("%d\n", m(2)); return 0; }
