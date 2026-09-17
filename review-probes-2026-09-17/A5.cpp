// A5 - #pragma pack is read and dropped without a word
// cl:    7 10 16
// cxx1i: 12 16 16
extern "C" int printf(const char *, ...);
#pragma pack(push, 1)
struct P1 { char c; int i; short s; };
#pragma pack(pop)
#pragma pack(2)
struct P2 { char c; double d; };
#pragma pack()
struct P3 { char c; double d; };
int main() { printf("%d %d %d\n", (int)sizeof(P1), (int)sizeof(P2), (int)sizeof(P3)); return 0; }
