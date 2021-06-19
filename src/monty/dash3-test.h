TEST_CASE("struct sizes") {
    CHECK(sizeof (ByteVec) == sizeof (VaryVec));
    CHECK(sizeof (void*) == sizeof (Value));
    CHECK(sizeof (void*) == sizeof (Object));
    CHECK(sizeof (void*) == sizeof (None));
    CHECK(sizeof (void*) == sizeof (Bool));
    CHECK(4 * sizeof (void*) == sizeof (Iterator));
    //FIXME CHECK(2 * sizeof (void*) + 8 == sizeof (Bytes));
    //FIXME CHECK(2 * sizeof (void*) + 8 == sizeof (Str));
    CHECK(4 * sizeof (void*) == sizeof (Lookup));
    //FIXME CHECK(2 * sizeof (void*) + 8 == sizeof (Tuple));
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

    //FIXME CHECK(2*sizeof (uint32_t) + 2*sizeof (void*) == sizeof (Buffer));
}
