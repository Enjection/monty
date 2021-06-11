#include <cstdint>
#include <cstring>

#include "vecs.h"

#include <cassert>

using namespace monty;

VecSlot *monty::vecLow;
VecSlot *monty::vecHigh;
uint8_t* monty::vecTop;

struct monty::VecSlot {
    auto isFree () const -> bool { return owner == nullptr; }
    auto payload () { return (uint8_t*) &next; }

    Vec* owner;
    VecSlot* next;
};

constexpr auto PSZ = sizeof (void*);
constexpr auto VSZ  = sizeof (VecSlot);
static_assert (VSZ == 2 * PSZ, "wrong VecSlot size");

static auto numVecSlots (size_t n) {
    return (n + VSZ - 1) / VSZ;
}

void monty::vecInit (void* base, size_t size) {
    //assert(size > 2 * VSZ);

    // to get alignment right, simply increase base and decrease size a bit
    // as a result, the allocated data itself sits on an xxxSlot boundary
    // this way no extra alignment is needed when setting up a memory pool
    while ((uintptr_t) base % VSZ != PSZ) {
        ++(uint8_t*&) base;
        --size;
    }
    size -= size % VSZ;

    vecLow = vecHigh = (VecSlot*) base;
    vecTop = (uint8_t*) base + size;
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
    assert((uintptr_t) &slot < (uintptr_t) vecTop);
    vecHigh = &slot;
    return true;
}

// turn a vec slot into a free slot, ending at tail
static void splitFreeVec (VecSlot& slot, VecSlot* tail) {
    if (tail <= &slot)
        return; // no room for a free slot
    slot.owner = nullptr;
    slot.next = tail;
}

auto Vec::slots () const -> uint32_t {
    return numVecSlots(_capa);
}

auto Vec::findSpace (uint32_t needs) -> void* {
    auto slot = (VecSlot*) vecLow;               // scan all vectors
    while (slot < vecHigh)
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
    if (slot == vecHigh) {
        if ((uintptr_t) (vecHigh + needs) > (uintptr_t) vecTop)
            assert(false);
            //return panicOutOfMemory(); // no space, and no room to expand
        vecHigh += needs;
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
        auto n = numVecSlots(sz + PSZ);
        if (n == numVecSlots(_capa))
            return true;
        _data = vecHigh->payload();
        vecHigh += n;
        sz = n * vec::VSZ - PSZ;
    }
    _capa = sz;
    return true;
}
#endif

union VecStats {
    struct {
        int
            checks, sweeps, compacts,
            toa, tob, tva, tvb, // total Obj/Vec Allocs/Bytes
            coa, cob, cva, cvb, // curr  Obj/Vec Allocs/Bytes
            moa, mob, mva, mvb; // max   Obj/Vec Allocs/Bytes
    };
    int v [15];
};
VecStats vecStats;

auto Vec::adj (size_t sz) -> bool {
    if (_data != nullptr && !inPool(_data))
        return false; // not resizable
    auto capas = slots();
    auto needs = sz > 0 ? numVecSlots(sz + PSZ) : 0U;
    if (capas != needs) {
        if (needs > capas)
            vecStats.tvb += (needs - capas) * VSZ;
        vecStats.cvb += (needs - capas) * VSZ;
        if (vecStats.mvb < vecStats.cvb)
            vecStats.mvb = vecStats.cvb;

        auto slot = _data != nullptr ? (VecSlot*) (_data - PSZ) : nullptr;
        if (slot == nullptr) {                  // new alloc
            ++vecStats.tva;
            ++vecStats.cva;
            if (vecStats.mva < vecStats.cva)
                vecStats.mva = vecStats.cva;

            slot = (VecSlot*) findSpace(needs);
            if (slot == nullptr)                // no room
                return false;
            _data = slot->payload();
        } else if (needs == 0) {                // delete
            --vecStats.cva;

            slot->owner = nullptr;
            slot->next = slot + capas;
            mergeVecs(*slot);
            _data = nullptr;
        } else {                                // resize
            auto tail = slot + capas;
            if (tail < vecHigh && tail->isFree())
                mergeVecs(*tail);
            if (tail == vecHigh) {              // easy resize
                if ((uintptr_t) (slot + needs) > (uintptr_t) vecTop)
                    //return panicOutOfMemory(), false;
                    assert(false);
                vecHigh += (int) (needs - capas);
            } else if (needs < capas)           // split, free at end
                splitFreeVec(slot[needs], slot + capas);
            else if (!tail->isFree() || slot + needs > tail->next) {
                // realloc, i.e. del + new
                auto nslot = (VecSlot*) findSpace(needs);
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
    //D( printf("\tcompacting ...\n"); )
    ++vecStats.compacts;
    auto newHigh = vecLow;
    uint32_t n;
    for (auto slot = newHigh; slot < vecHigh; slot += n)
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
    vecHigh = newHigh;
    assert((uintptr_t) vecHigh < (uintptr_t) vecTop);
}

#if DOCTEST
#include <doctest.h>
namespace {
#include "vecs-test.h"
}
#endif
