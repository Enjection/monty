TEST_CASE("data") {
    uint8_t memory [3*1024];
    //FIXME, vecs and objs share memory, but they may not overlap!!
    vecInit(memory, sizeof memory);
    objInit(memory, sizeof memory);
    uint32_t memAvail = 3000; (void) memAvail; //FIXME gcMax();

    SUBCASE("Int object tests") {
        // check that ints over ± 30 bits properly switch to Int objects
        static int8_t tests [] = { 29, 30, 31, 32, 63 };

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

    Object::sweep();
    Vec::compact();
    //FIXME CHECK(memAvail == gcMax());
}
