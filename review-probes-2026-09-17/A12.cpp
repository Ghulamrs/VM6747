// A12 - dynamic initialisation of a scalar at file scope is refused
// cl:    init 1 / 1
// cpp11: error: expected a constant initialiser, and this is not an integer constant expression
extern "C" int printf(const char *, ...);
int init(int v) { printf("init %d\n", v); return v; }
int gA = init(1);
int main() { printf("%d\n", gA); return 0; }
