TEST_CASE("Vec type") {
    Vec v;
    CHECK(v.cap() == 0);
    CHECK(v.ptr() == nullptr);
}

TEST_CASE("Vec init") {
    uint8_t mem [1000];

    SUBCASE("all") {
        vecInit(mem, sizeof mem);

        CHECK((uint8_t*) vecLow >= mem);
        CHECK(vecHigh >= vecLow);
        CHECK(vecTop > (uint8_t*) vecHigh);
        CHECK(vecTop <= mem + sizeof mem);
        CHECK(vecTop - (uint8_t*) vecLow <= sizeof mem);
        CHECK(vecTop - (uint8_t*) vecLow >= sizeof mem - 2 * sizeof (void*));

        CHECK((uintptr_t) vecLow % VSZ == PSZ);
        CHECK((uintptr_t) vecTop % VSZ == PSZ);
    }

    SUBCASE("offset") {
        for (int i = 0; i < 10; ++i) {
            vecInit(mem + i, sizeof mem - 10);

            CHECK((uint8_t*) vecLow >= mem);
            CHECK(vecHigh >= vecLow);
            CHECK(vecTop > (uint8_t*) vecHigh);
            CHECK(vecTop <= mem + sizeof mem);
            CHECK(vecTop - (uint8_t*) vecLow <= sizeof mem - 10);
            //CHECK(vecTop - vecLow >= sizeof mem - 10 - 2 * sizeof (void*));
        }
    }

    CHECK(vecHigh == vecLow);
}

TEST_CASE("Vec allocate") {
    uint8_t mem [1000];
    vecInit(mem, sizeof mem);

    Vec v;

    SUBCASE("empty vec") {
        v.adj(0);
        CHECK(v.cap() == 0);
        CHECK(v.ptr() == nullptr);
        CHECK(!Vec::inPool(v.ptr()));
    }

    SUBCASE("first vec") {
        v.adj(1);
        CHECK(v.cap() >= 1);
        CHECK(v.ptr() != nullptr);
        CHECK(Vec::inPool(v.ptr()));

        auto p = v.ptr();
        CHECK(p != nullptr);
        *p = 11;
        CHECK(*vecLow->payload() == 11);

        v.adj(1);
        CHECK(v.ptr() == p);

        v.adj(2);
        CHECK(v.cap() >= 2);
        CHECK(v.ptr() == p);

        v.adj(sizeof p);
        CHECK(v.cap() >= sizeof p);
        CHECK(v.ptr() == p);

        v.adj(sizeof p + 1);
        CHECK(v.cap() >= sizeof p + 1);
        CHECK(v.ptr() != nullptr);
        CHECK(v.ptr() == p); // resized in-place

        v.adj(0);
        CHECK(v.cap() == 0);
        CHECK(v.ptr() == nullptr);

        SUBCASE("second vec") {
            Vec w;

            w.adj(1);
            CHECK(w.cap() >= 1);
            CHECK(w.ptr() != nullptr);

            auto p = w.ptr();
            CHECK(p > (uint8_t*) vecLow);
            *p = 22;
            CHECK(*vecLow->payload() == 22); // reusing first slot
        }
    }

    SUBCASE("first vec again") {
        v.adj(1);
        CHECK(v.cap() >= 1);
        CHECK(v.ptr() != nullptr);

        auto p = v.ptr();
        *p = 33;
        CHECK(*vecLow->payload() == 33);

        v.adj(0);
    }

    Vec::compact();
    CHECK(vecHigh == vecLow);
}
