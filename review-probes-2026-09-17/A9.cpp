// A9 - <istream> >> int consumes the whole word, so what follows the digits is lost
// cl:    42 [abc] / 7 [tail]
// cxx1i: 42 [] / 7 [tail]
#include <sstream>
#include <iostream>
#include <string>
int main() { int x = 0; std::istringstream num("  42abc"); num >> x; std::string rest; num >> rest; std::cout << x << " [" << rest << "]" << std::endl;
    int y = 0; std::istringstream two("7 tail"); two >> y; std::string t; two >> t; std::cout << y << " [" << t << "]" << std::endl; return 0; }
