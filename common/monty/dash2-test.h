static char buf [250];
static char* bufEnd;

static void gcSetup (void* ptr, size_t len) {
    //FIXME, vecs and objs share memory, but they may not overlap!!
    vecInit(ptr, len);
    objInit(ptr, len);
}

struct TestBuffer : Buffer {
    TestBuffer () { bufEnd = buf; }
    ~TestBuffer () override {
        for (uint32_t i = 0; i < _fill && bufEnd < buf + sizeof buf - 1; ++i)
            *bufEnd++ = begin()[i];
        _fill = 0;
        *bufEnd = 0;
    }
};

TEST_CASE("repr") {
    uint8_t memory [3*1024];
    gcSetup(memory, sizeof memory);
    //FIXME uint32_t memAvail = gcMax();

    { TestBuffer tb; CHECK(buf == bufEnd); }
    CHECK("" == buf);

    { TestBuffer tb; tb.print("<%d>", 42); }
    CHECK("<42>" == buf);

    TestBuffer {} << Value () << ' ' << 123 << " abc " << (Value) "def";
    CHECK("_ 123 abc \"def\"" == buf);

    TestBuffer {} << Null << ' ' << True << ' ' << False;
    CHECK("null true false" == buf);

    TestBuffer {} << Object {};
    CHECK(strncmp("<<object> at ", buf, 13) == 0);

    TestBuffer {} << Buffer::info;
    CHECK("<type <buffer>>" == buf);

    { TestBuffer tb; tb << tb; }
    CHECK(strncmp("<<buffer> at ", buf, 13) == 0);

    Object::sweep();
    Vec::compact();
    //FIXME CHECK(memAvail == gcMax());
}

TEST_CASE("type") {
    uint8_t memory [3*1024];
    gcSetup(memory, sizeof memory);
    //FIXME uint32_t memAvail = gcMax();

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
        CHECK("defg" == (char const*) v.atGet(0));
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
    //FIXME CHECK(memAvail == gcMax());
}
