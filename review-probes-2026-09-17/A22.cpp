// A22 - std::move absent from <utility>
// cl:    runs
// cxx1i: error: 'std::move' was not declared - a prototype must come first
#include <utility>
extern "C" int printf(const char *, ...);
struct Buf {
    int *p; int n;
    Buf(int k) : p(new int[k]), n(k) { for (int i = 0; i < k; ++i) p[i] = i; printf("Buf(%d)\n", k); }
    Buf(const Buf &o) : p(new int[o.n]), n(o.n) { for (int i = 0; i < n; ++i) p[i] = o.p[i]; printf("copy\n"); }
    Buf(Buf &&o) : p(o.p), n(o.n) { o.p = 0; o.n = 0; printf("move\n"); }
    Buf &operator=(Buf &&o) { delete[] p; p = o.p; n = o.n; o.p = 0; o.n = 0; printf("move=\n"); return *this; }
    Buf &operator=(const Buf &o) { if (this != &o) { delete[] p; p = new int[o.n]; n = o.n; for (int i = 0; i < n; ++i) p[i] = o.p[i]; } printf("copy=\n"); return *this; }
    ~Buf() { delete[] p; }
};
int which(int &) { return 1; }
int which(int &&) { return 2; }
Buf build() { Buf b(4); return b; }
int main() {
    Buf a(3);
    Buf b(std::move(a));
    Buf c = build();
    Buf d(1);
    d = std::move(c);
    Buf e(2);
    e = b;
    int x = 0;
    int w1 = which(x); int w2 = which(1); int w3 = which(std::move(x));
    printf("%d %d %d %d %d %d\n", a.n, b.n, c.n, d.n, e.n, d.p[3]);
    printf("%d %d %d\n", w1, w2, w3);
    return 0;
}
