TEST_CASE("pyvm") {
    uint8_t memory [3*1024];
    //FIXME, vecs and objs share memory, but they may not overlap!!
    vecInit(memory, sizeof memory);
    objInit(memory, sizeof memory);
    uint32_t memAvail = 3000; (void) memAvail; //FIXME gcMax();

    CHECK(42 == 40 + 2); //TODO ...

    Object::sweep();
    Vec::compact();
    //FIXME CHECK(memAvail == gcMax());
}
