#include <cstdint>
#include <cstring>

#include "objs.h"

#include <cassert>

extern "C" int printf (char const*, ...);

using namespace monty;

#if VERBOSE_GC
#define D(x) { x; }
static void V (Obj const& o, char const* s) {
    Value((Obj const&) o).dump(s);
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

constexpr auto PTR_SZ = sizeof (void*);
constexpr auto OS_SZ  = sizeof (ObjSlot);

static_assert (OS_SZ == 2 * PTR_SZ, "wrong ObjSlot size");

static uintptr_t* start;    // start of memory pool, aligned to VS_SZ-PTR_SZ
static uintptr_t* limit;    // limit of memory pool, aligned to OS_SZ-PTR_SZ

static ObjSlot* objLow;     // low water mark of object memory pool
static ObjSlot* objBottom;  // high water mark of vector memory pool

union ObjStats {
    struct {
        int
            checks, sweeps, compacts,
            toa, tob, tva, tvb, // total Obj/Vec Allocs/Bytes
            coa, cob, cva, cvb, // curr  Obj/Vec Allocs/Bytes
            moa, mob, mva, mvb; // max   Obj/Vec Allocs/Bytes
    };
    int v [15];
};
ObjStats objStats;

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
    return Obj::inPool(&o) ? (ObjSlot*) ((uintptr_t) &o - PTR_SZ) : 0;
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

        ++objStats.toa;
        ++objStats.coa;
        if (objStats.moa < objStats.coa)
            objStats.moa = objStats.coa;
        objStats.tob += needs * OS_SZ;
        objStats.cob += needs * OS_SZ;
        if (objStats.mob < objStats.cob)
            objStats.mob = objStats.cob;

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

        if (objLow - needs < (void*) objBottom)
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

        --objStats.coa;
        objStats.cob -= (slot->chain - slot) * OS_SZ;

        slot->vt = nullptr; // mark this object as free and make it unusable

        mergeFreeObjs(*slot);

        // try to raise objLow, this will cascade when freeing during a sweep
        if (slot == objLow)
            objLow = objLow->chain;
    }

    void Obj::sweep () {
        D( printf("\tsweeping ...\n"); )
        ++objStats.sweeps;
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
                printf("dump: %p\n", slot->vt);
                //FIXME Value x {(Obj const&) slot->vt};
                //FIXME x.dump("");
            }
        }
    }

    void objInit (void* base, size_t size) {
        assert(size > 2 * OS_SZ);

        // to get alignment right, simply increase base and decrease size a bit
        // as a result, the allocated data itself sits on an xxxSlot boundary
        // this way no extra alignment is needed when setting up a memory pool

        while ((uintptr_t) base % OS_SZ != PTR_SZ) {
            base = (uint8_t*) base + 1;
            --size;
        }
        size -= size % OS_SZ;

        start = (uintptr_t*) base;
        limit = (uintptr_t*) ((uintptr_t) base + size);

        assert(start < limit); // need room for at least the objLow setup

        objBottom = (ObjSlot*) start;

        objLow = (ObjSlot*) limit - 1;
        objLow->chain = nullptr;
        objLow->vt = nullptr;

        //FIXME? assert((uintptr_t) &objBottom->next % OS_SZ == 0);
        assert((uintptr_t) &objLow->vt % OS_SZ == 0);

        D( printf("setup: start %p limit %p\n", start, limit); )
    }

    auto gcMax () -> int {
        return (uintptr_t) objLow - (uintptr_t) objBottom;
    }

    auto gcCheck () -> bool {
        ++objStats.checks;
        auto total = (intptr_t) limit - (intptr_t) start;
        return gcMax() < total / 4; // TODO crude
    }

    void mark (Obj const& obj) {
        if (Obj::inPool(&obj)) {
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

    void objReport () {
        printf("gc: max %d b, %d checks, %d sweeps, %d compacts\n",
                gcMax(), objStats.checks, objStats.sweeps, objStats.compacts);
        printf("gc: total %6d objs %8d b, %6d vecs %8d b\n",
                objStats.toa, objStats.tob, objStats.tva, objStats.tvb);
        printf("gc:  curr %6d objs %8d b, %6d vecs %8d b\n",
                objStats.coa, objStats.cob, objStats.cva, objStats.cvb);
        printf("gc:   max %6d objs %8d b, %6d vecs %8d b\n",
                objStats.moa, objStats.mob, objStats.mva, objStats.mvb);
    }
}

#if DOCTEST
#include <doctest.h>
namespace {
#include "objs-test.h"
}
#endif
