#include "monty.h"
#include "doctest.h"

using namespace monty;

TEST_CASE("monty") {
    uint8_t memory [3*1024];
    gcSetup(memory, sizeof memory);
    [[maybe_unused]] uint32_t memAvail = gcMax();

    SUBCASE("smoke test") {
        CHECK(42 == 40 + 2);
    }

    SUBCASE("struct sizes") {
        CHECK(sizeof (ByteVec) == sizeof (VaryVec));
        CHECK(sizeof (void*) == sizeof (Value));
        CHECK(sizeof (void*) == sizeof (Object));
        CHECK(sizeof (void*) == sizeof (None));
        CHECK(sizeof (void*) == sizeof (Bool));
        CHECK(4 * sizeof (void*) == sizeof (Iterator));
        CHECK(2 * sizeof (void*) + 8 == sizeof (Bytes));
        CHECK(2 * sizeof (void*) + 8 == sizeof (Str));
        CHECK(4 * sizeof (void*) == sizeof (Lookup));
        CHECK(2 * sizeof (void*) + 8 == sizeof (Tuple));
        CHECK(3 * sizeof (void*) + 8 == sizeof (Exception));

        // on 32b arch, packed = 12 bytes, which will fit in 2 GC slots i.s.o. 3
        // on 64b arch, packed is the same as unpacked as void* is also 8 bytes
        struct Packed { void* p; int64_t i; } __attribute__((packed));
        struct Normal { void* p; int64_t i; };
        CHECK((sizeof (Packed) < sizeof (Normal) || 8 * sizeof (void*) == 64));

        CHECK(sizeof (Packed) >= sizeof (Int));
        //CHECK(sizeof (Normal) <= sizeof (Int)); // fails on RasPi-32b

        // TODO incorrect formulas (size rounded up), but it works on 32b & 64b ...
        CHECK(2 * sizeof (void*) + 8 == sizeof (Range));
        CHECK(4 * sizeof (void*) == sizeof (Slice));
    }

    SUBCASE("Int object tests") {
        // check that ints over ± 30 bits properly switch to Int objects
        static int64_t tests [] = { 29, 30, 31, 32, 63 };

        for (auto e : tests) {
            int64_t pos = (1ULL << e) - 1;
            int64_t neg = - (1ULL << e);
            CHECK(pos > 0); // make sure there was no overflow
            CHECK(neg < 0); // make sure there was no underflow

            [[maybe_unused]] Value v = Int::make(pos);
            CHECK(pos == v.asInt());
            CHECK((e <= 30 ? v.isInt() : !v.isInt()));

            [[maybe_unused]] Value w = Int::make(neg);
            CHECK(neg == w.asInt());
            CHECK((e <= 30 ? w.isInt() : !w.isInt()));
        }
    }

    SUBCASE("varyVec tests") {
        VaryVec v;
        CHECK(0 == v.size());

        v.insert(0);
        CHECK(1 == v.size());
        CHECK(0 == v.atLen(0));

        v.atSet(0, "abc", 4);
        CHECK(1 == v.size());
        CHECK(4 == v.atLen(0));
        CHECK("abc" == (char const*) v.atGet(0));

        v.insert(0);
        CHECK(2 == v.size());
        CHECK(0 == v.atLen(0));
        CHECK(4 == v.atLen(1));
        CHECK("abc" == (char const*) v.atGet(1));

        v.atSet(0, "defg", 5);
        CHECK(5 == v.atLen(0));
        CHECK(4 == v.atLen(1));
        CHECK("defg" == (const char*) v.atGet(0));
        CHECK("abc" == (char const*) v.atGet(1));

        v.atSet(0, "hi", 3);
        CHECK(3 == v.atLen(0));
        CHECK(4 == v.atLen(1));
        CHECK("hi" == (char const*) v.atGet(0));
        CHECK("abc" == (char const*) v.atGet(1));

        v.atSet(0, nullptr, 0);
        CHECK(0 == v.atLen(0));
        CHECK(4 == v.atLen(1));
        CHECK("abc" == (char const*) v.atGet(1));

        v.remove(0);
        CHECK(1 == v.size());
        CHECK(4 == v.atLen(0));
        CHECK("abc" == (char const*) v.atGet(0));

        v.insert(1, 3);
        CHECK(4 == v.size());
        CHECK(4 == v.atLen(0));
        CHECK("abc" == (char const*) v.atGet(0));
        CHECK(0 == v.atLen(1));
        CHECK(0 == v.atLen(2));
        CHECK(0 == v.atLen(3));
        v.atSet(3, "four", 5);
        CHECK("four" == (char const*) v.atGet(3));
        v.atSet(2, "three", 6);
        CHECK("three" == (char const*) v.atGet(2));
        v.atSet(1, "two", 4);
        CHECK("two" == (char const*) v.atGet(1));
        v.atSet(0, "one", 4);
        CHECK("one" == (char const*) v.atGet(0));
        CHECK("two" == (char const*) v.atGet(1));
        CHECK("three" == (char const*) v.atGet(2));
        CHECK("four" == (char const*) v.atGet(3));

        v.remove(1, 2);
        CHECK(2 == v.size());
        CHECK(4 == v.atLen(0));
        CHECK(5 == v.atLen(1));
        CHECK("one" == (char const*) v.atGet(0));
        CHECK("four" == (char const*) v.atGet(1));
    }

    Object::sweep();
    Vec::compact();
    REQUIRE(memAvail == gcMax());
}
