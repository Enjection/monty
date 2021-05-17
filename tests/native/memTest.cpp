// TODO a first attempt to re-do the Vec & Obj classes with GC from scratch
// this code is not useful yet, and not used anywhere - just doodlin' ...

#include <cstdint>
#include <cstring>

namespace mem {
    uint8_t storage [1000];
    uint8_t* free = storage;

    struct Vec {
        auto cap () { return _capa; }
        auto ptr () { return _data; }
        void adj (size_t sz) {
            if (sz == _capa)
                return;
            if (sz == 0)
                _data = nullptr;
            else {
                _data = free;
                free += sz;
            }
            _capa = sz;
        }
    private:
        uint8_t* _data {nullptr};
        size_t _capa {0};
    };
}

#if DOCTEST
#include <doctest.h>

using namespace mem;

TEST_CASE("Vec type") {
    Vec v;
    CHECK(v.cap() == 0);
    CHECK(v.ptr() == nullptr);
}

TEST_CASE("allocate") {
    free = storage;
    Vec v;

    SUBCASE("empty vec") {
        v.adj(0);
        CHECK(v.cap() == 0);
        CHECK(v.ptr() == nullptr);
    }

    SUBCASE("first vec") {
        v.adj(1);
        CHECK(v.cap() >= 1);
        CHECK(v.ptr() != nullptr);

        auto p = v.ptr();
        *p = 11;
        CHECK(storage[0] == 11);

        v.adj(1);
        CHECK(v.ptr() == p);

        v.adj(2);
        CHECK(v.cap() >= 2);
        CHECK(v.ptr() != nullptr);
        CHECK(v.ptr() != p);

        v.adj(0);
        CHECK(v.cap() == 0);
        CHECK(v.ptr() == nullptr);

        SUBCASE("second vec") {
            Vec w;

            w.adj(1);
            CHECK(w.cap() >= 1);
            CHECK(w.ptr() != nullptr);

            auto p = w.ptr();
            CHECK(p > storage);
            *p = 22;
            CHECK(storage[0] == 11);
        }
    }

    SUBCASE("first vec again") {
        v.adj(1);
        CHECK(v.cap() >= 1);
        CHECK(v.ptr() != nullptr);

        auto p = v.ptr();
        *p = 33;
        CHECK(storage[0] == 33);
    }
}

#endif // DOCTEST
