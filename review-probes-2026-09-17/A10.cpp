// A10 - a label followed by a declaration is refused
// cl:    10
// cxx1i: error: a label cannot be followed by a declaration - put it in a block
extern "C" int printf(const char *, ...);
int main() {
    int j = 0;
    for (;;) { if (j == 3) goto out; ++j; }
out:
    int r = j * 2;
    printf("%d\n", r);
    return 0;
}
