// TODO a first attempt to re-do the Vec & Obj classes with GC from scratch
// this code is not useful yet, and not used anywhere - just doodlin' ...

#include <cstdint>
#include <cstring>

namespace mem {
    namespace vec {
        struct Slot *low, *high;
        uint8_t* top;

        void init (void* ptr, size_t len);
    }

    struct Vec {
        constexpr Vec () =default;
        ~Vec () { (void) adj(0); }

        static auto inPool (void const* p) -> bool {
            return vec::low < p && p < vec::high;
        }
        static void compact (); // reclaim and compact unused vector space

        auto cap () { return _capa; }
        auto ptr () { return _data; }
        auto adj (size_t sz) -> bool;
    private:
        uint8_t* _data {nullptr};
        size_t _capa {0};

        auto slots () const -> uint32_t; // capacity in vecslots
        auto findSpace (uint32_t) -> void*; // hidden private type

        // JT's "Rule of 5"
        Vec (Vec&&) =delete;
        Vec (Vec const&) =delete;
        auto operator= (Vec&&) -> Vec& =delete;
        auto operator= (Vec const&) -> Vec& =delete;
    };
}

constexpr auto PSZ = sizeof (void*);

namespace mem::vec {
    struct Slot {
        auto isFree () const -> bool { return owner == nullptr; }
        auto payload () { return (uint8_t*) &next; }

        Vec* owner;
        Slot* next;
    };

    constexpr auto VSZ  = sizeof (Slot);
    static_assert (VSZ == 2 * PSZ, "wrong Slot size");

    auto numSlots (size_t n) {
        return (n + VSZ - 1) / VSZ;
    }

    void init (void* base, size_t size) {
        //assert(size > 2 * VSZ);

        // to get alignment right, simply increase base and decrease size a bit
        // as a result, the allocated data itself sits on an xxxSlot boundary
        // this way no extra alignment is needed when setting up a memory pool
        while ((uintptr_t) base % VSZ != PSZ) {
            ++(uint8_t*&) base;
            --size;
        }
        size -= size % VSZ;

        low = high = (Slot*) base;
        top = (uint8_t*) base + size;
    }
}

using namespace mem;

#include <cassert>

// combine this free vec with all following free vecs
// return true if vec::high has been lowered, i.e. this free vec is now gone
static auto mergeVecs (vec::Slot& slot) -> bool {
    assert(slot.isFree());
    auto& tail = slot.next;
    while (tail < vec::high && tail->isFree())
        tail = tail->next;
    if (tail < vec::high)
        return false;
    assert((uintptr_t) &slot < (uintptr_t) vec::top);
    vec::high = &slot;
    return true;
}

// turn a vec slot into a free slot, ending at tail
static void splitFreeVec (vec::Slot& slot, vec::Slot* tail) {
    if (tail <= &slot)
        return; // no room for a free slot
    slot.owner = nullptr;
    slot.next = tail;
}

auto Vec::slots () const -> uint32_t {
    return vec::numSlots(_capa);
}

auto Vec::findSpace (uint32_t needs) -> void* {
    using namespace vec;

    auto slot = (Slot*) low;               // scan all vectors
    while (slot < high)
        if (!slot->isFree())                    // skip used slots
            slot += slot->owner->slots();
        else if (mergeVecs(*slot))              // no more free slots
            break;
        else if (slot + needs > slot->next)     // won't fit
            slot = slot->next;
        else {                                  // fits, may need to split
            splitFreeVec(slot[needs], slot->next);
            break;                              // found existing space
        }
    if (slot == high) {
        if ((uintptr_t) (high + needs) > (uintptr_t) vec::top)
            assert(false);
            //return panicOutOfMemory(); // no space, and no room to expand
        high += needs;
    }
    slot->owner = this;
    return slot;
}

#if 0
auto Vec::adj (size_t sz) -> bool {
    if (sz == _capa)
        return true;
    if (sz == 0)
        _data = nullptr;
    else {
        auto n = vec::numSlots(sz + PSZ);
        if (n == vec::numSlots(_capa))
            return true;
        _data = vec::high->payload();
        vec::high += n;
        sz = n * vec::VSZ - PSZ;
    }
    _capa = sz;
    return true;
}
#endif

union GCStats {
    struct {
        int
            checks, sweeps, compacts,
            toa, tob, tva, tvb, // total Obj/Vec Allocs/Bytes
            coa, cob, cva, cvb, // curr  Obj/Vec Allocs/Bytes
            moa, mob, mva, mvb; // max   Obj/Vec Allocs/Bytes
    };
    int v [15];
};
GCStats gcStats;

auto Vec::adj (size_t sz) -> bool {
    using namespace vec;

    if (_data != nullptr && !inPool(_data))
        return false; // not resizable
    auto capas = slots();
    auto needs = sz > 0 ? numSlots(sz + PSZ) : 0U;
    if (capas != needs) {
        if (needs > capas)
            gcStats.tvb += (needs - capas) * VSZ;
        gcStats.cvb += (needs - capas) * VSZ;
        if (gcStats.mvb < gcStats.cvb)
            gcStats.mvb = gcStats.cvb;

        auto slot = _data != nullptr ? (Slot*) (_data - PSZ) : nullptr;
        if (slot == nullptr) {                  // new alloc
            ++gcStats.tva;
            ++gcStats.cva;
            if (gcStats.mva < gcStats.cva)
                gcStats.mva = gcStats.cva;

            slot = (Slot*) findSpace(needs);
            if (slot == nullptr)                // no room
                return false;
            _data = slot->payload();
        } else if (needs == 0) {                // delete
            --gcStats.cva;

            slot->owner = nullptr;
            slot->next = slot + capas;
            mergeVecs(*slot);
            _data = nullptr;
        } else {                                // resize
            auto tail = slot + capas;
            if (tail < high && tail->isFree())
                mergeVecs(*tail);
            if (tail == high) {              // easy resize
                if ((uintptr_t) (slot + needs) > (uintptr_t) top)
                    //return panicOutOfMemory(), false;
                    assert(false);
                high += (int) (needs - capas);
            } else if (needs < capas)           // split, free at end
                splitFreeVec(slot[needs], slot + capas);
            else if (!tail->isFree() || slot + needs > tail->next) {
                // realloc, i.e. del + new
                auto nslot = (Slot*) findSpace(needs);
                if (nslot == nullptr)           // no room
                    return false;
                memcpy(nslot->payload(), _data, _capa); // copy data over
                _data = nslot->payload();
                slot->owner = nullptr;
                slot->next = slot + capas;
            } else                              // use (part of) next free
                splitFreeVec(slot[needs], tail->next);
        }
        // clear newly added bytes
        auto obytes = _capa;
        _capa = needs > 0 ? needs * VSZ - PSZ : 0;
        if (_capa > obytes)                      // clear added bytes
            memset(_data + obytes, 0, _capa - obytes);
    }
    assert((uintptr_t) _data % VSZ == 0);
    return true;
}

void Vec::compact () {
    using namespace vec;

    //D( printf("\tcompacting ...\n"); )
    ++gcStats.compacts;
    auto newHigh = low;
    uint32_t n;
    for (auto slot = newHigh; slot < high; slot += n)
        if (slot->isFree())
            n = slot->next - slot;
        else {
            n = slot->owner->slots();
            if (newHigh < slot) {
                slot->owner->_data = newHigh->payload();
                memmove(newHigh, slot, n * VSZ);
            }
            newHigh += n;
        }
    high = newHigh;
    assert((uintptr_t) high < (uintptr_t) top);
}

#if DOCTEST
#include <doctest.h>

TEST_CASE("Vec type") {
    Vec v;
    CHECK(v.cap() == 0);
    CHECK(v.ptr() == nullptr);
}

TEST_CASE("Vec init") {
    using namespace vec;
    uint8_t mem [1000];

    SUBCASE("all") {
        init(mem, sizeof mem);

        CHECK((uint8_t*) low >= mem);
        CHECK(high >= low);
        CHECK(top > (uint8_t*) high);
        CHECK(top <= mem + sizeof mem);
        CHECK(top - (uint8_t*) low <= sizeof mem);
        CHECK(top - (uint8_t*) low >= sizeof mem - 2 * sizeof (void*));

        CHECK((uintptr_t) low % VSZ == PSZ);
        CHECK((uintptr_t) top % VSZ == PSZ);
    }

    SUBCASE("offset") {
        for (int i = 0; i < 10; ++i) {
            init(mem + i, sizeof mem - 10);

            CHECK((uint8_t*) low >= mem);
            CHECK(high >= low);
            CHECK(top > (uint8_t*) high);
            CHECK(top <= mem + sizeof mem);
            CHECK(top - (uint8_t*) low <= sizeof mem - 10);
            //CHECK(top - low >= sizeof mem - 10 - 2 * sizeof (void*));
        }
    }

    CHECK(vec::high == vec::low);
}

TEST_CASE("Vec allocate") {
    using namespace vec;
    uint8_t mem [1000];
    init(mem, sizeof mem);

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
        CHECK(*low->payload() == 11);

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
            CHECK(p > (uint8_t*) low);
            *p = 22;
            CHECK(*low->payload() == 22); // reusing first slot
        }
    }

    SUBCASE("first vec again") {
        v.adj(1);
        CHECK(v.cap() >= 1);
        CHECK(v.ptr() != nullptr);

        auto p = v.ptr();
        *p = 33;
        CHECK(*low->payload() == 33);

        v.adj(0);
    }

    Vec::compact();
    CHECK(vec::high == vec::low);
}

#endif // DOCTEST
