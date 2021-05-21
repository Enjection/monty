#include <cstdint>
#include <cstring>

namespace buf {
    void init (void* ptr, size_t len);

    struct Buf {
        constexpr static auto SIZE = 512;

        Buf () =default;
        Buf (void* p);

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

        void verify () const;
    //private:
        Buf head {};
        uint8_t tail {0};
    };
}

#include <doctest.h>

namespace buf {
    uint8_t* bufs;
    Buf* tags;
    uint8_t nBufs;
    Queue free;

    void init (void* ptr, size_t len) {
        bufs = (uint8_t*) ptr;
        nBufs = len / Buf::SIZE;

        memset(bufs, 0, nBufs);
        tags = (Buf*) ptr;

        Buf b;
        for (int i = 1; i < nBufs; ++i) {
            b.id = i;
            free.append(b);
        }
    }

    Buf::Buf (void* p) : id (((uint8_t*) p - bufs) / nBufs) {}

    Buf::Buf (Buf& b) : id (b.id) { b.id = 0; }

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
        return tags[id];
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
        if (tail != 0)
            tags[tail] = b;
        tail = b.id;
        if (!head)
            head = b;
    }
}

using namespace buf;

TEST_CASE("buffers") {
    uint8_t mem [5000];
    init(mem, sizeof mem);

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
    CHECK(b.id != 0);
    CHECK(b != nullptr);

    Buf b2 = free.pull();
    CHECK(b2);
    CHECK(b2 != nullptr);
    CHECK(b.id != b2.id);

    uint8_t* p = b;
    CHECK(p != nullptr);

    b = {};
    CHECK(!b);
    CHECK(b.id == 0);

    uint8_t* q = b;
    CHECK(q == nullptr);
}

int main (int argc, char const** argv) {
    return doctest::Context(argc, argv).run();
}
