/* M10 W3: c90 under cdb - the C version of cpp11's w1 and w2.
   Gate: `m10.py oracle tests/m10/w3.c 27 --compiler c90` (innermost) and 33
   (the nested block) give the same stack and locals as cl /TC /Zi. */
#include <stdio.h>

struct point { int x; int y; };
enum colour { RED, GREEN = 5, BLUE };
union both { int i; float f; };

int total = 7;
static double scale = 2.5;

int inner(int x, char tag, double ratio) {
    static int calls = 0;
    const int base = 100;
    int y = x * 2;
    char c = tag;
    double d = ratio * 2.0;
    int *p = &y;
    const char *name = "inner";
    struct point pt;
    int arr[3];
    enum colour col = BLUE;
    union both u;
    calls = calls + 1;
    pt.x = x; pt.y = y; arr[0] = 1; arr[1] = 2; arr[2] = 3; u.i = 65;
    y = *p + base + c + (int)d + calls + (int)name[0] + pt.x + arr[2] + (int)col + u.i;
    {
        int nested = y + 1;
        long wide = 123456L;
        unsigned short small = 9;
        nested = nested + (int)wide + small;
        y = nested;
    }
    return y;
}

int middle(int x) {
    int r = inner(x + 1, 'A', scale);
    return r + 3;
}

int outer(int x) {
    int m = middle(x);
    int n = m * 10 + total;
    return n;
}

int main(void) {
    int v = outer(4);
    printf("%d\n", v);
    return 0;
}
