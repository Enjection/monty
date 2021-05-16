#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <setjmp.h>

namespace boss::pool {
    using Id_t = uint8_t;

    void init (void* ptr, size_t len);
    void deinit ();

    struct Buf {
        auto operator new (size_t bytes) -> void*;
        void operator delete (void*);

        auto id () const { return asId(this); }
        auto tag () -> uint8_t&;

        static auto asId (void const*) -> Id_t;
        static auto at (Id_t) -> Buf&;

        uint8_t data [512];
    };

    struct Queue {
        bool isEmpty () const { return head == 0; }

        auto pull () -> Id_t;
        void insert (Id_t id);
        void insert (void* p) { insert(Buf::asId(p)); }
        void append (Id_t id);
        void append (void* p) { append(Buf::asId(p)); }

        template <typename F>
        void remove (F f) {
            auto p = &head;
            while (*p != 0) {
                auto& next = tag(*p);
                if (f(*p)) {
                    if (tail == *p)
                        tail = p == &head ? 0 : p - &tag(0); // TODO yuck
                    *p = next;
                } else
                    p = &next;
            }
        }

        void verify () const;
    private:
        auto tag (Id_t id) const -> uint8_t& { return Buf::at(id).tag(); }

        Id_t head {0}, tail {0};
    };
}

namespace boss::pool {
    Buf* buffers;
    uint8_t numBufs;
    Queue freeBufs;

    void init (void* ptr, size_t len) {
        buffers = (Buf*) ptr;
        numBufs = len / sizeof (Buf);
        for (int i = 1; i < numBufs; ++i)
            freeBufs.append(i);
    }

    void deinit () {
        buffers = nullptr;
        numBufs = 0;
        freeBufs = {};
    }

    auto Buf::operator new (size_t bytes) -> void* {
        (void) bytes; // TODO check that it fits
        auto id = freeBufs.pull();
        //TODO check != 0
        return &Buf::at(id);
    }

    void Buf::operator delete (void* p) {
        freeBufs.insert((Buf*) p);
    }

    auto Buf::tag () -> uint8_t& {
        return buffers->data[this - buffers];
    }

    auto Buf::asId (void const* p) -> Id_t {
        return (Buf const*) p - buffers;
    }

    auto Buf::at (Id_t id) -> Buf& {
        return buffers[id];
    }

    auto Queue::pull () -> Id_t {
        auto id = head;
        if (id != 0) {
            head = tag(id);
            if (head == 0)
                tail = 0;
            tag(id) = 0;
        }
        return id;
    }

    void Queue::insert (Id_t id) {
        tag(id) = head;
        head = id;
        if (tail == 0)
            tail = head;
    }

    void Queue::append (Id_t id) {
        tag(id) = 0;
        if (tail != 0)
            tag(tail) = id;
        tail = id;
        if (head == 0)
            head = tail;
    }
}

//#define DOCTEST_CONFIG_DISABLE
#include "doctest.h"
#include <cstdio>
#include <cstring>

namespace boss::pool {
    doctest::String toString(Buf* p) {
        char buf [20];
        sprintf(buf, "Buf(%d)", (int) (p - buffers));
        return buf;
    }
}

using namespace boss;
using namespace pool;

void Queue::verify () const {
    if (head == 0 && tail == 0)
        return;
    CAPTURE((int) numBufs);
    Id_t tags [numBufs];
    memset(tags, 0, sizeof tags);
    for (auto curr = head; curr != 0; curr = tag(curr)) {
        CAPTURE((int) curr);
        CHECK(tags[curr] == 0);
        tags[curr] = 1;

        auto next = tag(curr);
        if (next == 0)
            CHECK(curr == tail);
        else
            CHECK(curr != tail);
    }
}

TEST_CASE("buffer") {
    uint8_t mem [5000];
    init(mem, sizeof mem);

    INFO("numBufs: ", (int) numBufs);

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

    auto& r = Buf::at(i);
    CHECK(&r == bp);

    freeBufs.verify();
    deinit();
}

TEST_CASE("pool") {
    uint8_t mem [5000];
    init(mem, sizeof mem);

    SUBCASE("new") {
        auto bp = new Buf;
        auto i = bp->id();
        CHECK(i == 1);
        CHECK(&Buf::at(i) == bp);

        auto bp2 = new Buf;
        CHECK(bp2->id() != 0);
        CHECK(bp2 != bp);
    }

    SUBCASE("reuse deleted") {
        auto p = new Buf;
        delete p;
        CHECK(new Buf == p);
    }

    SUBCASE("reuse in lifo order") {
        auto p = new Buf, q = new Buf, r = new Buf;
        delete p;
        delete q;
        delete r;
        CHECK(new Buf == r);
        CHECK(new Buf == q);
        CHECK(new Buf == p);
    }

    freeBufs.verify();
    deinit();
}

TEST_CASE("queue") {
    uint8_t mem [5000];
    init(mem, sizeof mem);

    Queue q;

    SUBCASE("insert one") {
        q.insert(new Buf);
        CHECK(q.pull() == 1);
        CHECK(q.isEmpty());
    }

    SUBCASE("insert many") {
        q.insert(new Buf);
        q.insert(new Buf);
        q.insert(new Buf);
        CHECK(q.pull() == 3);
        CHECK(q.pull() == 2);
        CHECK(q.pull() == 1);
        CHECK(q.isEmpty());
    }

    SUBCASE("append one") {
        q.append(new Buf);
        CHECK(q.pull() == 1);
        CHECK(q.isEmpty());
    }

    SUBCASE("append many") {
        q.append(new Buf);
        q.append(new Buf);
        q.append(new Buf);
        CHECK(q.pull() == 1);
        CHECK(q.pull() == 2);
        CHECK(q.pull() == 3);
        CHECK(q.isEmpty());
    }

    SUBCASE("remove") {
        for (int i = 0; i < 5; ++i) {
            auto p = new Buf;
            CHECK(p->id() == i + 1);
            q.append(p);
        }

        SUBCASE("none") {
            q.remove([](int) { return false; });
            for (int i = 0; i < 5; ++i)
                CHECK(q.pull() == i + 1);
            CHECK(q.isEmpty());
        }

        SUBCASE("one") {
            q.remove([](int id) { return id == 3; });
            q.verify();
            CHECK(q.pull() == 1);
            CHECK(q.pull() == 2);
            CHECK(q.pull() == 4);
            CHECK(q.pull() == 5);
            CHECK(q.isEmpty());
        }

        SUBCASE("even") {
            q.remove([](int id) { return id % 2 == 0; });
            q.verify();
            CHECK(q.pull() == 1);
            CHECK(q.pull() == 3);
            CHECK(q.pull() == 5);
            CHECK(q.isEmpty());
        }

        SUBCASE("all") {
            q.remove([](int) { return true; });
            q.verify();
            CHECK(q.isEmpty());
        }
    }

    freeBufs.verify();
    deinit();
}
