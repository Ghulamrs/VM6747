/* M10 W4, the C half: arrays, structs, unions, enums and typedefs in cdb.
   Gate: m10.py compare tests/m10/w4.c 30 --compiler c90 --expr ... equal to cl /TC /Zi. */
#include <stdio.h>

enum colour { RED, GREEN = 5, BLUE };
typedef unsigned int count;

struct point { int x; int y; };
union bits { int i; float f; };
struct shape { struct point corner; int sides[4]; enum colour hue; };

static int sum(const int *v, int n) {
    int s = 0;
    int i;
    for (i = 0; i < n; i++) s += v[i];
    return s;
}

int main(void) {
    int row[3];
    struct point pt;
    union bits b;
    enum colour c = GREEN;
    count n = 9;
    struct shape sh;
    char name[6] = "cdb";
    int t;
    row[0] = 1; row[1] = 2; row[2] = 3;
    pt.x = 4; pt.y = 5;
    b.i = 0x3f800000;
    sh.corner = pt; sh.sides[0] = 1; sh.sides[1] = 2; sh.sides[2] = 3; sh.sides[3] = 4; sh.hue = BLUE;
    t = sum(row, 3) + pt.x + (int)n;
    printf("%d %s %d %d\n", t, name, (int)c, sh.sides[3]);
    return t == 19 ? 0 : 1;
}
