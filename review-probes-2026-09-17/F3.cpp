// F3 - tms6747: std::map<int,double> reads back 0.0
// cl:    2.5 2.5 1.5 1 (arm64-darwin agrees)
// cpp11: 0.0 0.0 0.0 1 on vm6747
#include <map>
extern "C" int printf(const char *, ...);
int main() {
    std::map<int, double> md; md[3] = 1.5; md[1] = 2.5;
    std::map<int, double>::iterator it = md.begin();
    double d = it->second; double e = md[1]; double f = md[3];
    printf("%.1f %.1f %.1f %d\n", d, e, f, it->first);
    return 0;
}
