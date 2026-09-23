// A19 - char (*)[3] to const char (*)[3] qualification conversion refused
// cl:    c b 1
// cpp11: error: 'g' is 'const char [3] *' and this is 'char [3] *' - a cast says you meant it
extern "C" int printf(const char *, ...);
int main() { char grid[2][3] = {"ab", "cd"}; const char (*g)[3] = grid; char (*h)[3] = grid; const char *const *q = 0; char **r = 0; q = r; printf("%c %c %d\n", g[1][0], h[0][1], q == 0); return 0; }
