// A15 - partial ordering of f(T) against f(T*) reported ambiguous
// cl:    0 1
// cxx1i: error: this call to 'kind' is ambiguous
extern "C" int printf(const char *, ...);
template <class T> int kind(T) { return 0; }
template <class T> int kind(T *) { return 1; }
int main() { int z = 0; int a = kind(1); int b = kind(&z); printf("%d %d\n", a, b); return 0; }
