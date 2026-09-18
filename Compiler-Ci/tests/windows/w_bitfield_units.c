// expect: 52
// no-reference: the gcc on this box lays bit-fields out the Itanium way; cl is the reference, measured below
/* The Microsoft ABI allocates a bit-field in a unit of its declared type: a
   new unit, aligned to that type, when the type changes or the open unit is
   full, and the whole unit charged. Itanium packs end to end. Measured with
   cl 19.44 on 2026-09-18 (TriLab found the first shape at 8 here, 12 there):
     Mixed {char; unsigned:6; unsigned:6; int} 12   A {int:3; char:2}      8
     B {char:7; int:25}                          8   E {char; int:0; char} 2
     F {int:3; int:5}                            4   G {int:16; short:8}   8
     H {unsigned:31; unsigned:2}                 8   I {char:3; char:6}    2
   The sum is the answer; the fields must read back too. */
struct Mixed { char tag; unsigned int a : 6; unsigned int b : 6; int n; };
struct A { int a : 3; char b : 2; };
struct B { char a : 7; int b : 25; };
struct E { char a; int : 0; char b; };
struct F { int a : 3; int b : 5; };
struct G { int a : 16; short b : 8; };
struct H { unsigned a : 31; unsigned b : 2; };
struct I { char a : 3; char b : 6; };
int main(void)
{
    struct Mixed m;
    struct A a;
    int sum;
    m.tag = 'Z'; m.a = 63; m.b = 1; m.n = -5;
    a.a = -2; a.b = 1;
    if (m.tag != 'Z' || m.a != 63 || m.b != 1 || m.n != -5) return 1;
    if (a.a != -2 || a.b != 1) return 2;
    sum = (int)sizeof(struct Mixed) + (int)sizeof(struct A) + (int)sizeof(struct B) +
          (int)sizeof(struct E) + (int)sizeof(struct F) + (int)sizeof(struct G) +
          (int)sizeof(struct H) + (int)sizeof(struct I);
    return sum;
}
