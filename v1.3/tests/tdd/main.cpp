#include <utility>
#include <cstdio>
//#include <doctest.h>

int seq;

struct S {
    S () { printf("S%d ()\n", id); }
    S (S const& s) { printf("S%d (const& S%d)\n", id, s.id); }
    S (S&& s) { printf("S%d(&& S%d)\n", id, s.id); }
    S& operator= (S const& s) { printf("S%d = const& S%d\n", id, s.id); return *this; }
    S& operator= (S&& s) { printf("S%d = && S%d\n", id, s.id); return *this; }
    ~S () { printf("~S%d ()\n", id); }

    auto take () {
        S r;
        r.id = id;
        id = -id;
        return r;
    }

    int id = ++seq;
};

auto f () -> S { return {}; }
auto& g (S&& s) { return s; }

int main () {
    S s1;
    //S s2 = g(s1);
    S s2 = g(s1.take());
    //S s2 = g(std::move(s1));
    S s3 = s1;
}
