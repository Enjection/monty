// stack.cpp - execution state, stacks, and callables

#include "monty.h"
#include <cassert>

using namespace Monty;

volatile uint32_t Interp::pending;
Context* Interp::context;

void Context::enter (Callable const& func) {
    auto frameSize = func.code.fastSlotTop() + EXC_STEP * func.code.excLevel();
    int need = (frame().stack + frameSize) - (begin() + base);

    auto curr = base;           // current frame offset
    base = fill;                // new frame offset
    insert(fill, need);         // make room

    auto& f = frame();          // new frame
    f.base = curr;              // index of (now previous) frame
    f.sp = spIdx;               // previous stack index
    f.ip = ipIdx;               // previous instruction index
    f.callee = callee;          // previous callable

    spIdx = f.stack-begin()-1;  // stack starts out empty
    ipIdx = 0;                  // code starts at first opcode
    f.ep = 0;                   // no exceptions pending
    callee = &func;             // new callable context
}

Value Context::leave (Value v) {
    auto& f = frame();
    auto r = f.result;          // stored result
    if (r.isNil())              // use return result if set
        r = v;                  // ... else arg

    if (base > 0) {
        int prev = f.base;      // previous frame offset
        assert(prev >= 0);

        spIdx = f.sp;
        ipIdx = f.ip;
        callee = &f.callee.asType<Callable>();

        assert(fill > base);
        remove(base, fill - base); // delete current frame
        base = prev;            // new lower frame offset
    } else
        Interp::context = caller; // last frame, drop this stack context, restore caller

    return r;
}

auto Context::excBase (int incr) -> Value* {
    size_t ep = frame().ep;
    frame().ep = ep + incr;
    if (incr <= 0)
        --ep;
    return frame().stack + callee->code.fastSlotTop() + EXC_STEP * ep;
}

void Context::raise (Value exc) {
    uint32_t num = 0;
    if (exc.isInt()) {
        num = exc;      // trigger soft-irq 1..31 (interrupt-safe)
        assert(num < 8 * sizeof Interp::pending);
    } else
        event = exc;    // trigger exception or other outer-loop req

    Interp::interrupt(num);     // set pending, to force inner loop exit
}

void Context::caught () {
    auto e = event;
    event = {};
    if (e.isNil())
        return; // there was no exception, just an inner loop exit

    assert(frame().ep > 0); // simple exception, no stack unwind
    auto ep = excBase(0);
    ipIdx = ep[0];
    spIdx = ep[1];
    begin()[++spIdx] = e.isNil() ? ep[3] : e;
}

auto Context::next () -> Value {
    Interp::resume(*this);
    return {}; // no result yet
}

void Context::marker () const {
    List::marker();
    mark(callee);
    mark(event);
    mark(caller);
}

void Interp::resume (Context& ctx) {
    assert(context != &ctx);
    context = &ctx;
    interrupt(0); // force inner loop exit
}

void Interp::exception (Value v) {
    assert(!v.isInt());
    assert(context != nullptr);
    context->raise(v);
}

void Interp::interrupt (uint32_t num) {
    assert(num < 8 * sizeof pending);
    // see https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
    __atomic_fetch_or(&pending, 1U << num, __ATOMIC_RELAXED);
}

auto Interp::nextPending () -> int {
    if (pending != 0)
        for (size_t num = 0; num < 8 * sizeof pending; ++num)
            if (pendingBit(num))
                return num;
    return -1;
}

auto Interp::pendingBit (uint32_t num) -> bool {
    assert(num < 8 * sizeof pending);
    auto mask = 1U << num;
    // see https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
    return (__atomic_fetch_nand(&pending, mask, __ATOMIC_RELAXED) & mask) != 0;
}