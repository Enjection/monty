// gc.cpp - objects and vectors with garbage collection

#define VERBOSE_GC 0 // show garbage collector activity

#include "monty.h"
#include <cassert>

using namespace monty;

#if VERBOSE_GC
#define D(x) { x; }
static void V (Obj const& o, char const* s) {
    Value((Object const&) o).dump(s);
}
#else
#define D(...)
#define V(...)
#endif

struct ObjSlot {
    auto next () const -> ObjSlot* { return (ObjSlot*) (flag & ~1); }
    auto isFree () const -> bool   { return vt == nullptr; }
    auto isLast () const -> bool   { return chain == nullptr; }
    auto isMarked () const -> bool { return (flag & 1) != 0; }
    void setMark ()                { flag |= 1; }
    void clearMark ()              { flag &= ~1; }

    // field order is essential, vt must be last
    union {
        ObjSlot* chain;
        uintptr_t flag; // bit 0 set for marked objects
    };
    void* vt; // null for deleted objects
};

struct VecSlot {
    auto isFree () const -> bool { return vec == nullptr; }

    Vec* vec;
    union {
        VecSlot* next;
        uint8_t buf [1];
    };
};

constexpr auto PTR_SZ = sizeof (void*);
constexpr auto VS_SZ  = sizeof (VecSlot);
constexpr auto OS_SZ  = sizeof (ObjSlot);

static_assert (OS_SZ == 2 * PTR_SZ, "wrong ObjSlot size");
static_assert (VS_SZ == 2 * PTR_SZ, "wrong VecSlot size");
static_assert (OS_SZ == VS_SZ, "mismatched slot sizes");

static uintptr_t* start;    // start of memory pool, aligned to VS_SZ-PTR_SZ
static uintptr_t* limit;    // limit of memory pool, aligned to OS_SZ-PTR_SZ

static ObjSlot* objLow;     // low water mark of object memory pool
static VecSlot* vecHigh;    // high water mark of vector memory pool

GCStats monty::gcStats;

template< typename T >
static auto roundUp (uint32_t n) -> uint32_t {
    constexpr auto mask = sizeof (T) - 1;
    static_assert ((sizeof (T) & mask) == 0, "must be power of 2");
    return (n + mask) & ~mask;
}

template< typename T >
static auto multipleOf (uint32_t n) -> uint32_t {
    return roundUp<T>(n) / sizeof (T);
}

static auto obj2slot (Obj const& o) -> ObjSlot* {
    return o.isCollectable() ? (ObjSlot*) ((uintptr_t) &o - PTR_SZ) : 0;
}

static void mergeFreeObjs (ObjSlot& slot) {
    while (true) {
        auto nextSlot = slot.chain;
        assert(nextSlot != nullptr);
        if (!nextSlot->isFree() || nextSlot->isLast())
            break;
        slot.chain = nextSlot->chain;
    }
}

// combine this free vec with all following free vecs
// return true if vecHigh has been lowered, i.e. this free vec is now gone
static auto mergeVecs (VecSlot& slot) -> bool {
    assert(slot.isFree());
    auto& tail = slot.next;
    while (tail < vecHigh && tail->isFree())
        tail = tail->next;
    if (tail < vecHigh)
        return false;
    assert((uintptr_t) &slot < (uintptr_t) objLow);
    vecHigh = &slot;
    return true;
}

// turn a vec slot into a free slot, ending at tail
static void splitFreeVec (VecSlot& slot, VecSlot* tail) {
    if (tail <= &slot)
        return; // no room for a free slot
    slot.vec = nullptr;
    slot.next = tail;
}

// don't use lambda w/ assert, since Espressif's ESP8266 compiler chokes on it
// (hmmm, perhaps the assert macro is trying to obtain a function name ...)
//void* (*panicOutOfMemory)() = []() { assert(false); return nullptr; };
static void* defaultOutOfMemoryHandler () { assert(false); return nullptr; }

namespace monty {
    void* (*panicOutOfMemory)() = defaultOutOfMemoryHandler;

    auto Obj::inPool (void const* p) -> bool {
        return objLow < p && p < limit;
    }

    auto Obj::operator new (size_t sz) -> void* {
        auto needs = multipleOf<ObjSlot>(sz + PTR_SZ);

        ++gcStats.toa;
        ++gcStats.coa;
        if (gcStats.moa < gcStats.coa)
            gcStats.moa = gcStats.coa;
        gcStats.tob += needs * OS_SZ;
        gcStats.cob += needs * OS_SZ;
        if (gcStats.mob < gcStats.cob)
            gcStats.mob = gcStats.cob;

        // traverse object pool, merge free slots, loop until first fit
        for (auto slot = objLow; !slot->isLast(); slot = slot->next())
            if (slot->isFree()) {
                mergeFreeObjs(*slot);

                int slack = slot->chain - slot - needs;
                if (slack >= 0) {
                    if (slack > 0) { // put object at end of free space
                        slot->chain -= needs;
                        slot += slack;
                        slot->chain = slot + needs;
                    }
                    return &slot->vt;
                }
            }

        if (objLow - needs < (void*) vecHigh)
            return panicOutOfMemory(); // give up

        objLow -= needs;
        objLow->chain = objLow + needs;

        // new objects are always at least ObjSlot-aligned, i.e. 8-/16-byte
        assert((uintptr_t) &objLow->vt % OS_SZ == 0);
        return &objLow->vt;
    }

    void Obj::operator delete (void* p) {
        assert(p != nullptr);
        auto slot = obj2slot(*(Obj*) p);
        assert(slot != nullptr);

        --gcStats.coa;
        gcStats.cob -= (slot->chain - slot) * OS_SZ;

        slot->vt = nullptr; // mark this object as free and make it unusable

        mergeFreeObjs(*slot);

        // try to raise objLow, this will cascade when freeing during a sweep
        if (slot == objLow)
            objLow = objLow->chain;
    }

    void Obj::sweep () {
        D( printf("\tsweeping ...\n"); )
        ++gcStats.sweeps;
        for (auto slot = objLow; slot != nullptr; slot = slot->chain)
            if (slot->isMarked())
                slot->clearMark();
            else if (!slot->isFree()) {
                auto q = (Obj*) &slot->vt;
                V(*q, "\t delete");
                delete q;
                assert(slot->isFree());
            }
    }

    void Obj::dumpAll () {
        printf("objects: %p .. %p\n", objLow, limit);
        for (auto slot = objLow; slot != nullptr; slot = slot->chain) {
            if (slot->chain == 0)
                break;
            int bytes = (slot->chain - slot) * sizeof *slot;
            printf("od: %p %6d b :", slot, bytes);
            if (slot->isFree())
                printf(" free\n");
            else {
                Value x {(Object const&) slot->vt};
                x.dump("");
            }
        }
    }

    auto Vec::inPool (void const* p) -> bool {
        return start < p && p < vecHigh;
    }

    auto Vec::slots () const -> uint32_t {
        return multipleOf<VecSlot>(_capa);
    }

    auto Vec::findSpace (uint32_t needs) -> void* {
        auto slot = (VecSlot*) start;               // scan all vectors
        while (slot < vecHigh)
            if (!slot->isFree())                    // skip used slots
                slot += slot->vec->slots();
            else if (mergeVecs(*slot))              // no more free slots
                break;
            else if (slot + needs > slot->next)     // won't fit
                slot = slot->next;
            else {                                  // fits, may need to split
                splitFreeVec(slot[needs], slot->next);
                break;                              // found existing space
            }
        if (slot == vecHigh) {
            if ((uintptr_t) (vecHigh + needs) > (uintptr_t) objLow)
                return panicOutOfMemory(); // no space, and no room to expand
            vecHigh += needs;
        }
        slot->vec = this;
        return slot;
    }

    // many tricky cases, to merge/reuse free slots as much as possible
    auto Vec::adj (uint32_t sz) -> bool {
        if (_data != nullptr && !inPool(_data))
            return false; // not resizable
        auto capas = slots();
        auto needs = sz > 0 ? multipleOf<VecSlot>(sz + PTR_SZ) : 0U;
        if (capas != needs) {
            if (needs > capas)
                gcStats.tvb += (needs - capas) * VS_SZ;
            gcStats.cvb += (needs - capas) * VS_SZ;
            if (gcStats.mvb < gcStats.cvb)
                gcStats.mvb = gcStats.cvb;

            auto slot = _data != nullptr ? (VecSlot*) (_data - PTR_SZ) : nullptr;
            if (slot == nullptr) {                  // new alloc
                ++gcStats.tva;
                ++gcStats.cva;
                if (gcStats.mva < gcStats.cva)
                    gcStats.mva = gcStats.cva;

                slot = (VecSlot*) findSpace(needs);
                if (slot == nullptr)                // no room
                    return false;
                _data = slot->buf;
            } else if (needs == 0) {                // delete
                --gcStats.cva;

                slot->vec = nullptr;
                slot->next = slot + capas;
                mergeVecs(*slot);
                _data = nullptr;
            } else {                                // resize
                auto tail = slot + capas;
                if (tail < vecHigh && tail->isFree())
                    mergeVecs(*tail);
                if (tail == vecHigh) {              // easy resize
                    if ((uintptr_t) (slot + needs) > (uintptr_t) objLow)
                        return panicOutOfMemory(), false;
                    vecHigh += (int) (needs - capas);
                } else if (needs < capas)           // split, free at end
                    splitFreeVec(slot[needs], slot + capas);
                else if (!tail->isFree() || slot + needs > tail->next) {
                    // realloc, i.e. del + new
                    auto nslot = (VecSlot*) findSpace(needs);
                    if (nslot == nullptr)           // no room
                        return false;
                    memcpy(nslot->buf, _data, _capa); // copy data over
                    _data = nslot->buf;
                    slot->vec = nullptr;
                    slot->next = slot + capas;
                } else                              // use (part of) next free
                    splitFreeVec(slot[needs], tail->next);
            }
            // clear newly added bytes
            auto obytes = _capa;
            _capa = needs > 0 ? needs * VS_SZ - PTR_SZ : 0;
            if (_capa > obytes)                      // clear added bytes
                memset(_data + obytes, 0, _capa - obytes);
        }
        assert((uintptr_t) _data % VS_SZ == 0);
        return true;
    }

    void Vec::compact () {
        D( printf("\tcompacting ...\n"); )
        ++gcStats.compacts;
        auto newHigh = (VecSlot*) start;
        uint32_t n;
        for (auto slot = newHigh; slot < vecHigh; slot += n)
            if (slot->isFree())
                n = slot->next - slot;
            else {
                n = slot->vec->slots();
                if (newHigh < slot) {
                    slot->vec->_data = newHigh->buf;
                    memmove(newHigh, slot, n * VS_SZ);
                }
                newHigh += n;
            }
        vecHigh = newHigh;
        assert((uintptr_t) vecHigh < (uintptr_t) objLow);
    }

    void Vec::dumpAll () {
        printf("vectors: %p .. %p\n", start, vecHigh);
        uint32_t n;
        for (auto slot = (VecSlot*) start; slot < vecHigh; slot += n) {
            if (slot->isFree())
                n = slot->next - slot;
            else
                n = slot->vec->slots();
            printf("vd: %p %6d b :", slot, (int) (n * sizeof *slot));
            if (slot->isFree())
                printf(" free\n");
            else
                printf(" at %p\n", slot->vec);
        }
    }

    void gcSetup (void* base, uint32_t size) {
        assert(size > 2 * VS_SZ);

        // to get alignment right, simply increase base and decrease size a bit
        // as a result, the allocated data itself sits on an xxxSlot boundary
        // this way no extra alignment is needed when setting up a memory pool

        while ((uintptr_t) base % VS_SZ != PTR_SZ) {
            base = (uint8_t*) base + 1;
            --size;
        }
        size -= size % VS_SZ;

        start = (uintptr_t*) base;
        limit = (uintptr_t*) ((uintptr_t) base + size);

        assert(start < limit); // need room for at least the objLow setup

        vecHigh = (VecSlot*) start;

        objLow = (ObjSlot*) limit - 1;
        objLow->chain = nullptr;
        objLow->vt = nullptr;

        assert((uintptr_t) &vecHigh->next % VS_SZ == 0);
        assert((uintptr_t) &objLow->vt % OS_SZ == 0);

        D( printf("setup: start %p limit %p\n", start, limit); )
    }

    auto gcMax () -> int {
        return (uintptr_t) objLow - (uintptr_t) vecHigh;
    }

    auto gcCheck () -> bool {
        ++gcStats.checks;
        auto total = (intptr_t) limit - (intptr_t) start;
        return gcMax() < total / 4; // TODO crude
    }

    void mark (Obj const& obj) {
        if (obj.isCollectable()) {
            auto p = obj2slot(obj);
            if (p != nullptr) {
                if (p->isMarked())
                    return;
                V(obj, "\t mark");
                p->setMark();
            }
        }
        obj.marker();
    }

    void gcReport () {
        printf("gc: max %d b, %d checks, %d sweeps, %d compacts\n",
                gcMax(), gcStats.checks, gcStats.sweeps, gcStats.compacts);
        printf("gc: total %6d objs %8d b, %6d vecs %8d b\n",
                gcStats.toa, gcStats.tob, gcStats.tva, gcStats.tvb);
        printf("gc:  curr %6d objs %8d b, %6d vecs %8d b\n",
                gcStats.coa, gcStats.cob, gcStats.cva, gcStats.cvb);
        printf("gc:   max %6d objs %8d b, %6d vecs %8d b\n",
                gcStats.moa, gcStats.mob, gcStats.mva, gcStats.mvb);
    }
}

#if DOCTEST
#include <doctest.h>

int created, destroyed, marked, failed;

struct MarkObj : Object {
    MarkObj (Object* o =0) : other (o) { ++created; }
    ~MarkObj () override               { ++destroyed; }

private:
    void marker () const override      { ++marked; mark(other); }

    Object* other;
};

// expose some methods just for testing
struct TestVec : Vec {
    using Vec::ptr;
    using Vec::cap;
    using Vec::adj;
};

TEST_CASE("mem") {
    uint8_t memory [3*1024];
    gcSetup(memory, sizeof memory);
    uint32_t memAvail = gcMax();
    created = destroyed = marked = failed = 0;
    panicOutOfMemory = []() -> void* { ++failed; return nullptr; };

    SUBCASE("sizes") {
        CHECK(sizeof (void*) == sizeof (Object));
        CHECK(2 * sizeof (void*) == sizeof (Vec));
    }

    SUBCASE("init") {
        CHECK(sizeof memory > gcMax());
        CHECK(sizeof memory - 50 < gcMax());
    }

    SUBCASE("new") {
        MarkObj o1; // on the stack
        CHECK(!o1.isCollectable());
        CHECK(sizeof (MarkObj) == sizeof o1);
        CHECK(memAvail == gcMax());

        auto p1 = new MarkObj; // allocated in pool
        CHECK(p1 != nullptr);
        CHECK(p1->isCollectable());

        auto avail1 = gcMax();
        CHECK(memAvail > avail1);

        auto p2 = new MarkObj; // second object in pool
        CHECK(p2->isCollectable());
        CHECK(p1 != p2);

        auto avail2 = gcMax();
        CHECK(avail1 > avail2);
        CHECK(memAvail - avail1 == avail1 - avail2);

        auto p3 = new (0) MarkObj; // same as without the extra size
        CHECK(p3->isCollectable());

        auto avail3 = gcMax();
        CHECK(avail2 > avail3);
        CHECK(avail1 - avail2 == avail2 - avail3);

        auto p4 = new (20) MarkObj; // extra space at end of object
        CHECK(p4->isCollectable());

        auto avail4 = gcMax();
        CHECK(avail3 > avail4);
        CHECK(avail1 - avail2 < avail3 - avail4);
    }

    SUBCASE("markObj") {
        auto p1 = new MarkObj;
        CHECK(1 == created);

        auto p2 = new MarkObj (p1);
        CHECK(2 == created);

        mark(p2);
        CHECK(2 == marked);

        mark(p2);
        CHECK(2 == marked);

        Object::sweep();
        CHECK(0 == destroyed);

        mark(p2);
        CHECK(4 == marked); // now everything is marked again

        Object::sweep();
        CHECK(0 == destroyed);
        Object::sweep();
        CHECK(2 == destroyed);
    }

    SUBCASE("markThrough") {
        auto p1 = new MarkObj;
        MarkObj o1 (p1); // not allocated, but still traversed when marking
        auto p2 = new MarkObj (&o1);
        CHECK(3 == created);

        mark(p2);
        CHECK(3 == marked);

        mark(p2);
        CHECK(3 == marked);

        Object::sweep();
        CHECK(0 == destroyed);
        Object::sweep();
        CHECK(2 == destroyed);
    }

    SUBCASE("reuseObjs") {
        auto p1 = new MarkObj;          // [ p1 ]
        delete p1;                      // [ ]
        CHECK(memAvail == gcMax());

        auto p2 = new MarkObj;          // [ p2 ]
        /* p3: */ new MarkObj;          // [ p3 p2 ]
        delete p2;                      // [ p3 gap ]
        CHECK(p1 == p2);
        CHECK(memAvail > gcMax());

        auto p4 = new MarkObj;          // [ p3 p4 ]
        CHECK(p1 == p4);

        Object::sweep();                // [ ]
        CHECK(memAvail == gcMax());
    }

    SUBCASE("mergeNext") {
        auto p1 = new MarkObj;
        auto p2 = new MarkObj;
        /* p3: */ new MarkObj;          // [ p3 p2 p1 ]

        delete p1;
        delete p2;                      // [ p3 gap ]

        auto p4 = new (1) MarkObj;      // [ p3 p4 ]
        (void) p4; // TODO changed: CHECK(p2 == p4);
    }

    SUBCASE("mergePrevious") {
        auto p1 = new MarkObj;
        auto p2 = new MarkObj;
        auto p3 = new MarkObj;          // [ p3 p2 p1 ]

        delete p2;                      // [ p3 gap p1 ]
        delete p1;                      // [ p3 gap gap ]

        auto p4 = new (1) MarkObj;      // [ p3 p4 ]
        // TODO changed: CHECK(p2 == p4);

        delete p4;                      // [ p3 gap ]
        delete p3;                      // [ ]
        CHECK(memAvail == gcMax());
    }

    SUBCASE("mergeMulti") {
        auto p1 = new (100) MarkObj;
        auto p2 = new (100) MarkObj;
        auto p3 = new (100) MarkObj;
        /* p4: */ new (100) MarkObj;    // [ p4 p3 p2 p1 ]

        delete p3;                      // [ p4 gap p2 p1 ]
        delete p2;                      // [ p4 gap gap p1 ]
        delete p1;                      // [ p4 gap gap gap ]

        auto p5 = new (300) MarkObj;    // [ p4 p5 ]
        (void) p5; // TODO changed: CHECK(p3 == p5);
    }

    SUBCASE("outOfObjs") {
#if 0 // can't test this anymore, since panic can't return
        constexpr auto N = 400;
        for (int i = 0; i < 12; ++i)
            new (N) MarkObj;
        CHECK(N > gcMax());
        CHECK(12 == created);
        CHECK(5 == failed);

        destroyed += failed; // avoid assertion in tearDown()
#endif
    }

    SUBCASE("newVec") {
        {
            TestVec v1;
            CHECK(2 * sizeof (void*) == sizeof v1);
            CHECK(nullptr == v1.ptr());
            CHECK(0 == v1.cap());
            CHECK(memAvail == gcMax());
        }
        CHECK(memAvail == gcMax());
    }

    SUBCASE("resizeVec") {
        {
            TestVec v1;
            auto f = v1.adj(0);
            CHECK(f);
            CHECK(nullptr == v1.ptr());
            CHECK(0 == v1.cap());
            CHECK(memAvail == gcMax());

            f = v1.adj(1);
            CHECK(f);
            CHECK(nullptr != v1.ptr());
            CHECK(sizeof (void*) == v1.cap());
            CHECK(memAvail > gcMax());

            f = v1.adj(sizeof (void*));
            CHECK(f);
            CHECK(sizeof (void*) == v1.cap());

            f = v1.adj(sizeof (void*) + 1);
            CHECK(f);
            CHECK(3 * sizeof (void*) == v1.cap());

#if !NATIVE // FIXME crashes since change from size_t to uint32_t ???
            f = v1.adj(1);
            CHECK(f);
            CHECK(sizeof (void*) == v1.cap());
#endif
        }
        CHECK(memAvail == gcMax());
    }

    SUBCASE("reuseVecs") {
        TestVec v1;
        v1.adj(100);                    // [ v1 ]
        TestVec v2;
        v2.adj(20);                     // [ v1 v2 ]

        auto a = gcMax();
        CHECK(memAvail > a);
        CHECK(v1.ptr() < v2.ptr());

        v1.adj(0);                      // [ gap v2 ]
        CHECK(a == gcMax());

        TestVec v3;
        v3.adj(20);                     // [ v3 gap v2 ]
        CHECK(a == gcMax());

        TestVec v4;
        v4.adj(20);                     // [ v3 v4 gap v2 ]
        CHECK(a == gcMax());

        v3.adj(0);                      // [ gap v4 gap v2 ]
        v4.adj(0);                      // [ gap gap v2 ]
        CHECK(a == gcMax());

        TestVec v5;
        v5.adj(100);                    // [ v5 v2 ]
        CHECK(a == gcMax());

        v5.adj(40);                     // [ v5 gap v2 ]
        CHECK(a == gcMax());

        v5.adj(80);                     // [ v5 gap v2 ]
        CHECK(a == gcMax());

        v5.adj(100);                    // [ v5 v2 ]
        CHECK(a == gcMax());

        v5.adj(20);                     // [ v5 gap v2 ]
        v1.adj(1);                      // [ v5 v1 gap v2 ]
        CHECK(a == gcMax());

        v1.adj(0);                      // [ v5 gap v2 ]
        v5.adj(0);                      // [ gap v2 ]

        v2.adj(0);                      // [ gap ]
        auto b = gcMax();
        CHECK(a < b);
        CHECK(memAvail > b);

        v1.adj(1);                      // [ v1 ]
        CHECK(b < gcMax());

        v1.adj(0);                      // [ ]
        CHECK(memAvail == gcMax());
    }

    SUBCASE("compactVecs") {
        TestVec v1;
        v1.adj(20);                     // [ v1 ]
        TestVec v2;
        v2.adj(20);                     // [ v1 v2 ]

        auto a = gcMax();

        TestVec v3;
        v3.adj(20);                     // [ v1 v2 v3 ]

        auto b = gcMax();

        TestVec v4;
        v4.adj(20);                     // [ v1 v2 v3 v4 ]
        TestVec v5;
        v5.adj(20);                     // [ v1 v2 v3 v4 v5 ]

        auto c = gcMax();
        CHECK(b > c);

        Vec::compact();                 // [ v1 v2 v3 v4 v5 ]
        CHECK(c == gcMax());

        v2.adj(0);                      // [ v1 gap v3 v4 v5 ]
        v4.adj(0);                      // [ v1 gap v3 gap v5 ]
        CHECK(c == gcMax());

        Vec::compact();                 // [ v1 v3 v5 ]
        CHECK(b == gcMax());

        v1.adj(0);                      // [ gap v3 v5 ]
        CHECK(b == gcMax());

        Vec::compact();                 // [ v3 v5 ]
        CHECK(a == gcMax());
    }

    SUBCASE("vecData") {
        {
            TestVec v1;                 // fill with 0xFF's
            v1.adj(1000);
            CHECK(1000 <= v1.cap());

            memset(v1.ptr(), 0xFF, v1.cap());
            CHECK(0xFF == v1.ptr()[0]);
            CHECK(0xFF == v1.ptr()[v1.cap()-1]);
        }

        TestVec v2;
        v2.adj(20);                     // [ v2 ]
        auto p2 = v2.ptr();
        auto n = v2.cap();
        CHECK(p2 != nullptr);
        CHECK(20 <= n);

        p2[0] = 1;
        p2[n-1] = 2;
        CHECK(1 == p2[0]);
        CHECK(0 == p2[1]);
        CHECK(0 == p2[n-2]);
        CHECK(2 == p2[n-1]);

        v2.adj(40);                     // [ v2 ]
        CHECK(p2 == v2.ptr());
        CHECK(1 == p2[0]);
        CHECK(2 == p2[n-1]);
        CHECK(0 == p2[n]);
        CHECK(0 == p2[ v2.cap()-1]);

        v2.ptr()[n] = 11;
        v2.ptr()[v2.cap()-1] = 22;
        CHECK(11 == v2.ptr()[n]);
        CHECK(22 == v2.ptr()[v2.cap()-1]);

        TestVec v3;
        v3.adj(20);                     // [ v2 v3 ]
        auto p3 = v3.ptr();
        p3[0] = 3;
        p3[n-1] = 4;
        CHECK(3 == p3[0]);
        CHECK(4 == p3[n-1]);

        TestVec v4;
        v4.adj(20);                     // [ v2 v3 v4 ]
        auto p4 = v4.ptr();
        p4[0] = 5;
        p4[n-1] = 6;
        CHECK(5 == p4[0]);
        CHECK(6 == p4[n-1]);

        v3.adj(40);                     // [ v2 gap v4 v3 ]
        CHECK(p3 != v3.ptr());
        CHECK(3 == v3.ptr()[0]);
        CHECK(4 == v3.ptr()[n-1]);
        CHECK(0 == v3.ptr()[n]);
        CHECK(0 == v3.ptr()[v3.cap()-1]);

        v3.ptr()[n] = 33;
        v3.ptr()[v3.cap()-1] = 44;
        CHECK(33 == v3.ptr()[n]);
        CHECK(44 == v3.ptr()[v3.cap()-1]);

        auto a = gcMax();
        Vec::compact();                 // [ v2 v4 v3 ]
        CHECK(a < gcMax());

        CHECK(p2 == v2.ptr());
        CHECK(p3 == v4.ptr());
        CHECK(p4 == v3.ptr());

        CHECK(1 == p2[0]);
        CHECK(22 == p2[ v2.cap()-1]);

        CHECK(3 == v3.ptr()[0]);
        CHECK(44 == v3.ptr()[v3.cap()-1]);

        CHECK(5 == v4.ptr()[0]);
        CHECK(6 == v4.ptr()[n-1]);

        v2.adj(0);                      // [ gap v4 v3 ]
        v4.adj(0);                      // [ gap gap v3 ]

        Vec::compact();                 // [ v3 ]
        CHECK(memAvail > gcMax());

        CHECK(p2 == v3.ptr());

        CHECK(3 == v3.ptr()[0]);
        CHECK(44 == v3.ptr()[v3.cap()-1]);
    }

    SUBCASE("outOfVecs") {
        TestVec v1;
        auto f = v1.adj(999);
        CHECK(f);
        CHECK(0 == failed);

        auto p = v1.ptr();
        auto n = v1.cap();
        auto a = gcMax();
        CHECK(nullptr != p);
        CHECK(999 < n);
        CHECK(memAvail > a);

        f = v1.adj(sizeof memory); // fail, vector should be the old one

        CHECK(!f);
        CHECK(0 < failed);
        CHECK(p == v1.ptr());
        CHECK(n == v1.cap());
        CHECK(a == gcMax());
    }

    SUBCASE("gcRomOrRam") {
#if 0 // can't test on native
        struct RomObj {
            virtual void blah () {} // virtual is romable
        };
        static const RomObj romObj;
        static       RomObj ramObj;

        struct DataObj {
            virtual ~DataObj () {} // but not if virtual destructor (!)
        };
        static const DataObj dataObj;

        struct BssObj { // with plain destructor, it needs run-time init (?)
            ~BssObj () {}
        };
        static const BssObj bssObj;

        //extern int _etext [];
        extern int _sdata [];
        extern int _edata [];
        extern int _sbss [];
        extern int _ebss [];

        auto rom = (void*) &romObj;
        CHECK(rom < _sdata);                  // in flash, i.e. .rodata

        auto ram = (void*) &ramObj;                 // not const
        CHECK(_sdata <= ram && ram < _edata); // pre-inited, i.e. in .data

        auto data = (void*) &dataObj;
        CHECK(_sdata <= data && data < _edata);

        auto bss = (void*) &bssObj;
        CHECK(_sbss <= bss && bss < _ebss);
#endif

        // work around the fact that Obj::Obj() is protected
        Object stackObj;
        auto heapObj = new Object;

        auto heap = (void*) heapObj;
        CHECK(memory <= heap);
        CHECK(heap < memory + sizeof memory);

        auto stack = (void*) &stackObj;
        // heap is on the stack as well
        //CHECK(heap <= stack);
        CHECK(heap > stack);
    }

    Object::sweep();
    Vec::compact();
    CHECK(memAvail == gcMax());
}

#endif // DOCTEST
