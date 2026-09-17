// A1-caller - the cl-compiled half of the A1 pair
#include "A1.h"
extern "C" int fflush(void *); extern "C" void *__acrt_iob_func(unsigned);
int main() {
    printf("start\n"); fflush(__acrt_iob_func(1));
    NT nt = makeNT(14); printf("nt %d\n", nt.x % 1000); fflush(__acrt_iob_func(1));
    int r13 = takeNT(nt); printf("took %d\n", r13);
    return 0;
}
