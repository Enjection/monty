#include <cstdint>
#include <cstring>

namespace buf {
    void init (void* ptr, size_t len);

    struct Buf {
        constexpr static auto SIZE = 512;

        Buf () =default;
        Buf (void* p);
        ~Buf();

        Buf (Buf& b);
        auto operator= (Buf&) -> Buf&;
        Buf (const Buf&) =delete;
        auto operator= (Buf const&) -> Buf& =delete;
        Buf (Buf&& b) =default;
        auto operator= (Buf&&) -> Buf& =default;

        operator bool () const { return id != 0; }
        operator uint8_t* () const;

        auto next () -> Buf&;

        uint8_t id {0};
    };

    struct Queue {
        auto isEmpty () const -> bool { return !head; }

        auto pull () -> Buf;
        void insert (Buf);
        void append (Buf);

        void dump (char const*) const;
    //private:
        Buf head {};
        uint8_t tail {0};
    };
}

#include <doctest.h>
#include <cstdio>

namespace buf {
    uint8_t* bufs;
    Buf* chain;
    uint8_t nBufs;
    Queue free;

    void init (void* ptr, size_t len) {
        bufs = (uint8_t*) ptr;
        nBufs = len / Buf::SIZE;

        memset(bufs, 0, nBufs);
        chain = (Buf*) ptr;

        Buf b;
        for (int i = 1; i < nBufs; ++i) {
            b.id = i;
            free.append(b);
        }
    }

    Buf::Buf (void* p) : id (((uint8_t*) p - bufs) / nBufs) {}

    Buf::Buf (Buf& b) : id (b.id) { b.id = 0; }

    Buf::~Buf() {
        if (id != 0)
            free.insert(*this);
    }

    auto Buf::operator= (Buf& b) -> Buf& {
        if (id != 0)
            free.insert(*this);
        id = b.id;
        b.id = 0;
        return *this;
    }

    Buf::operator uint8_t* () const {
        return id != 0 ? bufs + id * SIZE : nullptr;
    }

    auto Buf::next () -> Buf& {
        return chain[id];
    }

    auto Queue::pull () -> Buf {
        Buf b;
        if (head) {
            b = head;
            head = b.next();
            if (!head)
                tail = 0;
        }
        return b;
    }

    void Queue::insert (Buf b) {
        b.next() = head;
        head = b;
        if (tail == 0)
            tail = head.id;
    }

    void Queue::append (Buf b) {
        b.next() = {};
        auto& slot = head ? chain[tail] : head;
        slot = b;
        tail = slot.id;
    }

    void Queue::dump (char const* msg) const {
        for (int i = head.id; i != 0; i = chain[i].id) {
            printf("%s %d", msg, i);
            msg = ",";
        }
        printf("\n");
    }
}

using namespace buf;

TEST_CASE("buffers") {
    uint8_t mem [5000];
    init(mem, sizeof mem);
    //free.dump("init");

    SUBCASE("allocate and release") {
        Buf b = free.pull();
        CHECK(b);
        CHECK(b.id == 1);
        CHECK(b != nullptr);
        CHECK(free.head.id == 2);
    }

    SUBCASE("allocate and copy") {
        CAPTURE((int) nBufs);
        CAPTURE((int) free.head.id);
        CAPTURE((int) free.tail);

        CHECK(nBufs == sizeof mem / Buf::SIZE);
        CHECK(!free.isEmpty());
        CHECK(free.head.id == 1);

        Buf b;
        CHECK(!b);
        CHECK(b.id == 0);
        CHECK(b == nullptr);

        b = free.pull();
        CHECK(b);
        CHECK(b.id == 1);
        CHECK(b != nullptr);
        // b gets release here, because it's not used anymore, only replaced (!)
        CHECK(free.head.id == 1);

        Buf b2 = free.pull();
        CHECK(b2);
        CHECK(b2.id == 1);
        CHECK(b2 != nullptr);
        CHECK(free.head.id == 2);

        b = b2;

        CHECK(b);
        CHECK(b.id == 1);
        CHECK(!b2);
        CHECK(b2.id == 0);
        CHECK(free.head.id == 1);

        b = {};
        CHECK(!b);
        CHECK(b.id == 0);
        CHECK(free.head.id == 1);
    }

    SUBCASE("free in different order") {
        {
            Buf b1 = free.pull();
            Buf b2 = free.pull();
            Buf b3 = free.pull();
            CHECK(free.head.id == 4);

            CHECK(b1.id == 1);
            CHECK(b2.id == 2);
            CHECK(b3.id == 3);

            b3 = b1; // b3=#1, b1=0, #3 freed
            b1 = b2; // b1=#2, b2=0
            b2 = b3; // b2=#1, b3=0

            CHECK(free.head.id == 3);
            // on scope exit, b2 will be freed (#1), then b1 (#2)
        }
        CHECK(free.head.id == 2);
    }

    //free.dump("done");
}

int main (int argc, char const** argv) {
    return doctest::Context(argc, argv).run();
}
