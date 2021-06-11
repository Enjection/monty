// array.cpp - arrays, dicts, and other derived types

#include "monty.h"
#include "typ-array.h"
#include <cassert>

using namespace monty;

Lookup const Array::attrs;

struct Accessor {
    virtual auto get (ByteVec&, uint32_t) const -> Value  = 0;
    virtual void set (ByteVec&, uint32_t, Value) const = 0;
    virtual void ins (ByteVec&, uint32_t, uint32_t) const = 0;
    virtual void del (ByteVec&, uint32_t, uint32_t) const = 0;
};

template< typename T >
static auto asVecOf (ByteVec& a) -> VecOf<T>& {
    return (VecOf<T>&) a;
}

template< typename T, int L =0 >
struct AccessAs : Accessor {
    auto get (ByteVec& vec, uint32_t pos) const -> Value override {
        if (L)
            return Int::make(asVecOf<T>(vec)[pos]);
        else
            return asVecOf<T>(vec)[pos];
    }
    void set (ByteVec& vec, uint32_t pos, Value val) const override {
        if (L)
            asVecOf<T>(vec)[pos] = val.asInt();
        else
            asVecOf<T>(vec)[pos] = val;
    }
    void ins (ByteVec& vec, uint32_t pos, uint32_t num) const override {
        asVecOf<T>(vec).insert(pos, num);
    }
    void del (ByteVec& vec, uint32_t pos, uint32_t num) const override {
        asVecOf<T>(vec).remove(pos, num);
    }
};

template< int L >                                   // 0 1 2
struct AccessAsBits : Accessor {                    // P T N
    constexpr static auto bits = 1 << L;            // 1 2 4
    constexpr static auto mask = (1 << bits) - 1;   // 1 3 15
    constexpr static auto shft = 3 - L;             // 3 2 1
    constexpr static auto rest = (1 << shft) - 1;   // 7 3 1

    auto get (ByteVec& vec, uint32_t pos) const -> Value override {
        return (vec[pos>>shft] >> bits * (pos & rest)) & mask;
    }
    void set (ByteVec& vec, uint32_t pos, Value val) const override {
        auto b = bits * (pos & rest);
        auto& e = vec[pos>>shft];
        e = (e & ~(mask << b)) | (((int) val & mask) << b);
    }
    void ins (ByteVec& vec, uint32_t pos, uint32_t num) const override {
        assert((pos & rest) == 0 && (num & rest) == 0);
        vec._fill >>= shft;
        vec.insert(pos >> shft, num >> shft);
        vec._fill <<= shft;
    }
    void del (ByteVec& vec, uint32_t pos, uint32_t num) const override {
        assert((pos & rest) == 0 && (num & rest) == 0);
        vec._fill >>= shft;
        vec.remove(pos >> shft, num >> shft);
        vec._fill <<= shft;
    }
};

struct AccessAsVaryBytes : Accessor {
    auto get (ByteVec& vec, uint32_t pos) const -> Value override {
        auto& v = (VaryVec&) vec;
        return new Bytes (v.atGet(pos), v.atLen(pos));
    }
    void set (ByteVec& vec, uint32_t pos, Value val) const override {
        auto& v = (VaryVec&) vec;
        if (val.isNil())
            v.remove(pos);
        else {
            auto& b = val.asType<Bytes>();
            v.atSet(pos, b.begin(), b.size());
        }
    }
    void ins (ByteVec& vec, uint32_t pos, uint32_t num) const override {
        ((VaryVec&) vec).insert(pos, num);
    }
    void del (ByteVec& vec, uint32_t pos, uint32_t num) const override {
        ((VaryVec&) vec).remove(pos, num);
    }
};

struct AccessAsVaryStr : AccessAsVaryBytes {
    auto get (ByteVec& vec, uint32_t pos) const -> Value override {
        return new Str ((char const*) ((VaryVec&) vec).atGet(pos));
    }
    void set (ByteVec& vec, uint32_t pos, Value val) const override {
        auto& v = (VaryVec&) vec;
        if (val.isNil())
            v.remove(pos);
        else {
            char const* s = val.isStr() ? val : val.asType<Str>();
            v.atSet(pos, (char const*) s, strlen(s) + 1);
        }
    }
};

constexpr char arrayModes [] = "PTNbBhHiIlLqvV"
#if USE_FLOAT
                            "f"
#endif
#if USE_DOUBLE
                            "d"
#endif
;

// permanent per-type accessors, these are re-used for every Array instance
// the cost per get/set/ins/del is one table index step, just as with vtables

static AccessAsBits<0>      const accessor_P;
static AccessAsBits<1>      const accessor_T;
static AccessAsBits<2>      const accessor_N;
static AccessAs<int8_t>     const accessor_b;
static AccessAs<uint8_t>    const accessor_B;
static AccessAs<int16_t>    const accessor_h;
static AccessAs<uint16_t>   const accessor_H;
static AccessAs<int32_t,1>  const accessor_l;
static AccessAs<uint32_t,1> const accessor_L;
static AccessAs<int64_t,1>  const accessor_q;
static AccessAsVaryBytes    const accessor_v;
static AccessAsVaryStr      const accessor_V;
#if USE_FLOAT
static AccessAs<float>      const accessor_f;
#endif
#if USE_DOUBLE
static AccessAs<double>     const accessor_d;
#endif

// must be in same order as arrayModes
static Accessor const* accessors [] = {
    &accessor_P,
    &accessor_T,
    &accessor_N,
    &accessor_b,
    &accessor_B,
    &accessor_h,
    &accessor_H,
    &accessor_h, // i, same as h
    &accessor_H, // I, same as H
    &accessor_l,
    &accessor_L,
    &accessor_q,
    &accessor_v,
    &accessor_V,
#if USE_FLOAT
    &accessor_f,
#endif
#if USE_DOUBLE
    &accessor_d,
#endif
};

static_assert (sizeof arrayModes - 1 <= 1 << (32 - Array::LEN_BITS),
                "not enough bits for accessor modes");
static_assert (sizeof arrayModes - 1 == sizeof accessors / sizeof *accessors,
                "incorrect number of accessors");

Array::Array (char type, uint32_t len) {
    auto p = strchr(arrayModes, type);
    auto s = p != nullptr ? p - arrayModes : 0; // use Value if unknown type
    accessors[s]->ins(*this, 0, len);
    _fill |= s << LEN_BITS;
}

auto Array::mode () const -> char {
    return arrayModes[_fill >> LEN_BITS];
}

auto Array::len () const -> uint32_t {
    return size() & ((1 << LEN_BITS) - 1);
}

auto Array::getAt (Value k) const -> Value {
    if (!k.isInt())
        return sliceGetter(k);
    auto n = k; // TODO relPos(k);
    return accessors[sel()]->get(const_cast<Array&>(*this), n);
}

auto Array::setAt (Value k, Value v) -> Value {
    if (!k.isInt())
        return sliceSetter(k, v);
    auto n = k; // TODO relPos(k);
    auto s = sel();
    _fill &= 0x07FFFFFF;
    accessors[s]->set(*this, n, v);
    _fill |= s << LEN_BITS;
    return {};
}

void Array::insert (uint32_t idx, uint32_t num) {
    auto s = sel();
    _fill &= 0x07FFFFFF;
    accessors[s]->ins(*this, idx, num);
    _fill = _fill + (s << LEN_BITS);
}

void Array::remove (uint32_t idx, uint32_t num) {
    auto s = sel();
    _fill &= 0x07FFFFFF;
    accessors[s]->del(*this, idx, num);
    _fill += s << LEN_BITS;
}

auto Array::copy (Range const& r) const -> Value {
    auto n = r.len();
    auto v = new Array (mode(), n);
    for (uint32_t i = 0; i < n; ++i)
        v->setAt(i, getAt(r.getAt(i)));
    return v;
}

auto Array::store (Range const& r, Object const& v) -> Value {
    assert(r._by == 1);
    int olen = r.len();
    int nlen = v.len();
    if (nlen < olen)
        remove(r._from + nlen, olen - nlen);
    else if (nlen > olen)
        insert(r._from + olen, nlen - olen);
    for (int i = 0; i < nlen; ++i)
        setAt(r.getAt(i), v.getAt(i));
    return {};
}

auto Array::create (ArgVec const& args, Type const*) -> Value {
    assert(args.size() >= 1 && args[0].isStr());
    char type = *((char const*) args[0]);
    uint32_t len = 0;
    if (args.size() == 2) {
        assert(args[1].isInt());
        len = args[1];
    }
    return new Array (type, len);
}

void Array::repr (Buffer& buf) const {
    auto m = mode();
    auto n = len();
    buf.print("%d%c", n, m);
    switch (m) {
        case 'q':                               n <<= 1; // fall through
        case 'l': case 'L':                     n <<= 1; // fall through
        case 'h': case 'H': case 'i': case 'I': n <<= 1; // fall through
        case 'b': case 'B':                     break;
        case 'P':                               n >>= 1; // fall through
        case 'T':                               n >>= 1; // fall through
        case 'N':                               n >>= 1; break;
        case 'v': case 'V': n = ((uint16_t const*) begin())[len()]; break;
    }
    auto p = (uint8_t const*) begin();
    for (uint32_t i = 0; i < n; ++i)
        buf.print("%02x", p[i]);
}

#if DOCTEST
#include <doctest.h>

TEST_CASE("array") {
    uint8_t memory [3*1024];
    gcSetup(memory, sizeof memory);
    uint32_t memAvail = gcMax();
    VecOf<int> v;

    CHECK(0 == v.cap());
    v.adj(25);

    for (int i = 0; i < 20; ++i)
        CHECK(0 == v[i]);

    CHECK(0 == v[10]);
    for (int i = 0; i < 10; ++i)
        v.begin()[i] = 11 * i;

    SUBCASE("vecOfTypeSizes") {
        CHECK(2 * sizeof (void*) == sizeof (Vec));
        CHECK(1 * sizeof (void*) + 8 == sizeof (VecOf<int>));
        CHECK(1 * sizeof (void*) + 8 == sizeof (VecOf<Vec>));
    }

    SUBCASE("vecOfInited") {
        CHECK(25 <= v.cap());
        CHECK(30 > v.cap());

        for (int i = 0; i < 10; ++i)
            CHECK(11 * i == v[i]);
        CHECK(0 == v[10]);
    }

    SUBCASE("vecOfMoveAndWipe") {
        v.move(2, 3, 4);

        static int m1 [] { 0, 11, 22, 33, 44, 55, 22, 33, 44, 99, 0, };
        for (int i = 0; i < 11; ++i)
            CHECK(m1[i] == v[i]);

        v.wipe(2, 3);

        static int m2 [] { 0, 11, 0, 0, 0, 55, 22, 33, 44, 99, 0, };
        for (int i = 0; i < 11; ++i)
            CHECK(m2[i] == v[i]);

        v.move(4, 3, -2);

        static int m3 [] { 0, 11, 0, 55, 22, 55, 22, 33, 44, 99, 0, };
        for (int i = 0; i < 11; ++i)
            CHECK(m3[i] == v[i]);
    }

    SUBCASE("vecOfCopyMove") {
        VecOf<int> v2;
        v2.adj(3);

        v2[0] = 100;
        v2[1] = 101;
        v2[2] = 102;

        CHECK(3 <= v2.cap());
        CHECK(8 > v2.cap());
    }

    SUBCASE("arrayTypeSizes") {
        CHECK(2 * sizeof (void*) + 8 == sizeof (Array));
        CHECK(2 * sizeof (void*) + 8 == sizeof (List));
        CHECK(2 * sizeof (void*) + 8 == sizeof (Set));
        CHECK(3 * sizeof (void*) + 8 == sizeof (Dict));
        CHECK(5 * sizeof (void*) + 8 == sizeof (Type));
        CHECK(5 * sizeof (void*) + 8 == sizeof (Class));
        CHECK(3 * sizeof (void*) + 8 == sizeof (Inst));
    }

    SUBCASE("arrayInsDel") {
        static struct { char typ; int64_t min, max; int log; } tests [] = {
            //    typ                     min  max                log
            { 'P',                      0, 1,                   0 },
            { 'T',                      0, 3,                   1 },
            { 'N',                      0, 15,                  2 },
            { 'b',                   -128, 127,                 3 },
            { 'B',                      0, 255,                 3 },
            { 'h',                 -32768, 32767,               4 },
            { 'H',                      0, 65535,               4 },
            { 'i',                 -32768, 32767,               4 },
            { 'I',                      0, 65535,               4 },
            { 'l',            -2147483648, 2147483647,          5 },
            { 'L',                      0, 4294967295,          5 },
            { 'q', -9223372036854775807-1, 9223372036854775807, 6 },
        };
        for (auto e : tests) {
            //printf("e %c min %lld max %lld log %d\n", e.typ, e.min, e.max, e.log);

            Array a (e.typ);
            CHECK(0 == a.len());

            constexpr auto N = 24;
            a.insert(0, N);
            CHECK(N == a.len());

            int bytes = ((N << e.log) + 7) >> 3;
            CHECK(bytes <= a.cap());
            CHECK(bytes + 2 * sizeof (void*) >= a.cap());

            for (uint32_t i = 0; i < a.len(); ++i)
                CHECK(0 == (int) a.getAt(i));

            a.setAt(0, Int::make(e.min));
            a.setAt(1, Int::make(e.min - 1));
            a.setAt(N-2, Int::make(e.max + 1));
            a.setAt(N-1, Int::make(e.max));

            CHECK(e.min == a.getAt(0).asInt());
            CHECK(e.max == a.getAt(N-1).asInt());

            // can't test for overflow using int64_t when storing ± 63-bit ints
            if (e.max != 9223372036854775807) {
                CHECK(e.min - 1 != a.getAt(1));
                CHECK(e.max + 1 != a.getAt(N-2));
                CHECK(e.min - 1 != a.getAt(1).asInt());
                CHECK(e.max + 1 != a.getAt(N-2).asInt());
            }

            a.remove(8, 8);
            CHECK(N-8 == a.len());
            CHECK(e.min == a.getAt(0).asInt());
            CHECK(e.max == a.getAt(N-8-1).asInt());

            a.insert(0, 8);
            CHECK(N == a.len());
            CHECK(0 == (int) a.getAt(0));
            CHECK(0 == (int) a.getAt(7));
            CHECK(e.min == a.getAt(8).asInt());
            CHECK(e.max == a.getAt(N-1).asInt());
        }
    }

    SUBCASE("listInsDel") {
        List l;
        CHECK(0 == l.size());

        l.insert(0, 5);
        CHECK(5 == l.size());

        for (auto e : l)
            CHECK(e.isNil());

        for (uint32_t i = 0; i < 5; ++i)
            l[i] = 10 + i;

        for (auto& e : l) {
            auto i = &e - &l[0];
            CHECK(10 + i == e);
        }

        l.insert(2, 3);
        CHECK(8 == l.size());

        static int m1 [] { 10, 11, 0, 0, 0, 12, 13, 14, };
        for (auto& e : m1) {
            auto i = &e - m1;
            CHECK(e == l[i]);
        }

        l.remove(1, 5);
        CHECK(3 == l.size());

        static int m2 [] { 10, 13, 14, };
        for (auto& e : m2) {
            auto i = &e - m2;
            CHECK(e == l[i]);
        }

        for (auto& e : l) {
            auto i = &e - &l[0];
            CHECK(m2[i] == e);
        }
    }

    SUBCASE("setInsDel") {
        Set s;
        CHECK(0 == s.size());

        for (int i = 20; i < 25; ++i)
            s.has(i) = true;
        CHECK(5 == s.size());     // 20 21 22 23 24

        CHECK(!s.has(19));
        CHECK(s.has(20));
        CHECK(s.has(24));
        CHECK(!s.has(25));

        for (int i = 20; i < 25; ++i)
            CHECK(s.has(i));

        for (auto e : s) {
            CHECK(20 <= (int) e);
            CHECK((int) e < 25);
        }

        for (int i = 23; i < 28; ++i)
            s.has(i) = false;
        CHECK(3 == s.size());     // 20 21 22

        CHECK(!s.has(19));
        CHECK(s.has(20));
        CHECK(s.has(22));
        CHECK(!s.has(23));

        for (int i = 20; i < 23; ++i)
            CHECK(s.has(i));

        for (int i = 19; i < 22; ++i)
            s.has(i) = true;
        CHECK(4 == s.size());     // 19 20 21 22

        CHECK(!s.has(18));
        CHECK(s.has(19));
        CHECK(s.has(22));
        CHECK(!s.has(23));

        for (int i = 19; i < 23; ++i)
            CHECK(s.has(i));

        s.has("abc") = true;
        s.has("def") = true;
        CHECK(6 == s.size());     // 19 20 21 22 "abc" "def"

        CHECK(!s.has(""));
        CHECK(s.has("abc"));
        CHECK(s.has("def"));
        CHECK(!s.has("ghi"));

        s.has("abc") = true; // no effect
        s.has("ghi") = false; // no effect
        CHECK(6 == s.size());     // 19 20 21 22 "abc" "def"

        s.has("def") = false;
        CHECK(5 == s.size());     // 19 20 21 22 "abc"
        CHECK(s.has("abc"));
        CHECK(!s.has("def"));
        CHECK(!s.has("ghi"));
    }

    SUBCASE("dictInsDel") {
        Dict d;
        CHECK(0 == d.size());

        for (int i = 0; i < 5; ++i)
            d.at(10+i) = 30+i;
        CHECK(5 == d.size());     // 10:30 11:31 12:32 13:33 14:34

        CHECK(d.has(12));
        CHECK(!d.has(15));

        Value v;
        v = d.at(12);
        CHECK(!v.isNil());
        CHECK(v.isInt()); // exists
        v = d.at(15);
        CHECK(v.isNil()); // doesn't exist

        for (int i = 0; i < 5; ++i) {
            Value e = d.at(10+i);
            CHECK(30+i == e);
        }

        d.at(15) = 35;
        CHECK(6 == d.size());     // 10:30 11:31 12:32 13:33 14:34 15:35
        CHECK(d.has(15));

        for (int i = 0; i < 6; ++i) {
            Value e = d.at(10+i);
            CHECK(30+i == e);
        }

        d.at(12) = 42;
        CHECK(6 == d.size());     // 10:30 11:31 12:42 13:33 14:34 15:35
        CHECK(d.has(12));
        CHECK(42 == (Value) d.at(12));

        d.at(11) = Value {};
        CHECK(5 == d.size());     // 10:30 12:42 13:33 14:34 15:35
        CHECK(!d.has(11));

        static int m1 [] { 1030, 1242, 1333, 1434, 1535, };
        for (auto e : m1) {
            int k = e / 100, v = e % 100;
            CHECK(v == (Value) d.at(k));
        }

#if 0
        auto p = d.begin(); // a sneaky way to access the underlying VecOf<Value>
        for (uint32_t i = 0; i < 2 * d.size(); ++i)
            printf("%d, ", (int) p[i]);
        printf("\n");
#endif
    }

    CHECK(0 < v.cap());
    v.adj(0);
    CHECK(0 == v.cap());

    Object::sweep();
    Vec::compact();
    CHECK(memAvail == gcMax());
}

#endif // DOCTEST
