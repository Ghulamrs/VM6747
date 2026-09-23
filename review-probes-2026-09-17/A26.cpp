// A26 - pointer compared with an int
// cl:    cl refuses: C2446
// cpp11: compiles
extern "C" int printf(const char *, ...);
int main() { int *p = 0; if (p == 1) printf("eq\n"); printf("x\n"); return 0; }
