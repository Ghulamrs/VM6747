// A8 - <ostream> prints unsigned char and signed char as integers
// cl:    B C D
// cxx1i: 66 67 68
#include <iostream>
int main() { unsigned char u = 66; signed char s = 67; std::cout << u << ' ' << s << ' ' << (unsigned char)68 << std::endl; return 0; }
