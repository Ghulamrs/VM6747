// A3-caller - the other half of the A3 pair
#include "A3.h"
struct Der : Virt { int extra; Der() : extra(50) {} int vf(int k) { return k + extra; } int g(int k) { return 1000 + k; } int g(double d) { return 2000 + (int)d; } ~Der() { printf("~Der\n"); } };
struct DP : Plain2 { int a() { return 7; } int c() { return 9; } };
int main() {
    printf("start\n");
    Der d; int cv = callVirt(&d, 3); Virt *v = makeVirt(); int cv2 = callVirt(v, 4); int uv = useVirt(v); Virt *dv = new Der(); int uv2 = useVirt(dv);
    printf("virt %d %d %d %d\n", cv, cv2, uv, uv2);
    DP dp; int cp = callPlain(&dp); Plain2 *pp = makePlain(); int a1 = pp->a(); int b1 = pp->b(); int c1 = pp->c(); delete pp;
    Virt *mv = makeVirt(); int g1 = mv->g(1); int g2 = mv->g(1.5); delete mv;
    printf("plain %d %d %d %d %d %d\n", cp, a1, b1, c1, g1, g2);
    return 0;
}
