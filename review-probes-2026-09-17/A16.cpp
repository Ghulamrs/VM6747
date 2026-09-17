// A16 - an out-of-line static data member is replayed for a partial specialization's instantiation
// cl:    4 10 3
// cxx1i: error: 'Box<int>::count' is defined twice
extern "C" int printf(const char *, ...);
template <class T> struct Box { T v; Box(T x) : v(x) {} T get() const { return v; } static int count; };
template <class T> int Box<T>::count = 0;
template <class T> struct Box<T *> { T *p; Box(T *x) : p(x) {} T get() const { return *p + 1; } };
int main() { Box<int> bi(4); int z = 9; Box<int *> bp(&z); typedef Box<int> BI; BI::count = 3; printf("%d %d %d\n", bi.get(), bp.get(), BI::count); return 0; }
