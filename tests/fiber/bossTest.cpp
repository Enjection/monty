#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <setjmp.h>

namespace newboss::pool {
    using Id_t = uint8_t;

    void init (void* ptr, size_t len);
    void deinit ();

    struct Buf {
        auto operator new (size_t bytes) -> void*;
        void operator delete (void*);

        auto id () const { return asId(this); }
        auto tag () -> uint8_t&;

        static auto asId (void const*) -> Id_t;
        static auto at (Id_t) -> Buf*;

        uint8_t data [512];
    };

    struct Queue {
        bool isEmpty () const { return head == 0; }
        auto pull () -> Id_t;
        void append (Id_t id);
        void append (void* p) { append(Buf::asId(p)); }
    private:
        Id_t head {0}, tail {0};
    };
}

namespace newboss::pool {
    Buf* buffers;
    uint8_t numBufs;
    uint8_t nextBuf;

    auto Buf::operator new (size_t bytes) -> void* {
        (void) bytes; // TODO check that it fits
        return buffers + ++nextBuf;
    }

    void Buf::operator delete (void* p) {
        (void) p; // TODO
    }

    auto Buf::tag () -> uint8_t& {
        return buffers->data[this - buffers];
    }

    auto Buf::asId (void const* p) -> Id_t {
        return (Buf const*) p - buffers;
    }

    auto Buf::at (Id_t id) -> Buf* {
        return buffers + id;
    }

    void init (void* ptr, size_t len) {
        buffers = (Buf*) ptr;
        numBufs = len / sizeof (Buf);
        nextBuf = 0;
    }

    void deinit () {
        buffers = nullptr;
        numBufs = nextBuf = 0;
    }

    auto Queue::pull () -> Id_t {
        auto id = head;
        if (id != 0) {
            head = Buf::at(id)->tag();
            if (head == 0)
                tail = 0;
            //Buf::at(id)->tag() = 0;
        }
        return id;
    }

    void Queue::append (Id_t id) {
        //TODO Buf::at(id)->tag() = 0;
        if (tail != 0)
            Buf::at(tail)->tag() = id;
        tail = id;
        if (head == 0)
            head = tail;
    }
}

#include "doctest.h"
#include <cstdio>

using namespace newboss;

namespace newboss::pool {
    doctest::String toString(Buf* p) {
        char buf [20];
        sprintf(buf, "Buf(%d)", (int) (p - buffers));
        return buf;
    }
}

TEST_CASE("buffer") {
    using namespace pool;

    auto bp = new Buf;
    CHECK(bp != nullptr);
    CHECK(bp->id() != 0);

    Queue q;
    CHECK(q.isEmpty());

    CHECK(q.pull() == 0);

    q.append(bp);
    CHECK(!q.isEmpty());

    auto i = q.pull();
    CHECK(i != 0);
    CHECK(q.isEmpty());

    auto r = Buf::at(i);
    CHECK(r != nullptr);
}

TEST_CASE("pool") {
    using namespace pool;

    uint8_t mem [10'000];
    init(mem, sizeof mem);

    SUBCASE("new") {
        auto bp = new Buf;
        auto i = bp->id();
        CHECK(i == 1);
        CHECK(Buf::at(i) == bp);

        delete bp;
        auto bp2 = new Buf;
        CHECK(bp2->id() != 0);
        //CHECK(bp2 == bp);
    }

    deinit();
}

TEST_CASE("queue") {
    using namespace pool;

    uint8_t mem [10'000];
    init(mem, sizeof mem);

    Queue q;

    SUBCASE("append one") {
        q.append(new Buf);
        CHECK(q.pull() == 1);
    }

    SUBCASE("append many") {
        q.append(new Buf);
        q.append(new Buf);
        q.append(new Buf);
        CHECK(q.pull() == 1);
        CHECK(q.pull() == 2);
        CHECK(q.pull() == 3);
    }

    deinit();
}
