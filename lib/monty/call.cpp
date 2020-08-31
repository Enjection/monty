// call.cpp - execution state, stacks, and callables

#include "monty.h"
#include <cassert>

using namespace Monty;

#ifdef UNIT_TEST
static auto archTime () -> uint32_t { assert(false); }
#else
extern auto archTime () -> uint32_t;
#endif

volatile uint32_t Interp::pending;
uint32_t          Interp::queueIds;
List              Interp::tasks;
Dict              Interp::modules;
Context*          Interp::context;

static uint32_t deadlines [Interp::MAX_QUEUES]; // when to wake up handlers
static uint32_t idleFlags [Interp::MAX_QUEUES]; // requirements while idling

Callable::Callable (Value bc, Tuple* t, Dict* d, Module* mod)
        : mo (mod != nullptr ? *mod : Interp::context->globals()),
          code (bc.asType<Bytecode>()), pos (t), kw (d) {
}

auto Callable::call (ArgVec const& args) const -> Value {
    auto ctx = Interp::context;
    auto coro = code.isGenerator();
    if (coro)
        ctx = new Context;

    ctx->enter(*this);

    int nPos = code.numArgs(0);
    int nDef = code.numArgs(1);
    int nKwo = code.numArgs(2);

//Value x = pos; x.dump("pos");
    for (int i = 0; i < nPos; ++i)
        if (i < args.num)
            ctx->fastSlot(i) = args[i];
        else if (pos != nullptr && (size_t) i < nDef + pos->fill)
            ctx->fastSlot(i) = (*pos)[i+nDef-nPos];

    if (code.hasVarArgs())
        ctx->fastSlot(nPos+nKwo) =
            Tuple::create({args.vec, args.num-nPos, args.off+nPos});

    // TODO this isn't quite right, inside bc, there's a list of indices ...
    //  but why not lazily turn the first deref load into a new cell?
    //  ... or keep a bitmap of which fast slots are / should be cells?
    for (size_t i = 0; i < code.numCells(); ++i)
        ctx->fastSlot(i) = new Cell (ctx->fastSlot(i));

    return coro ? ctx : Value {};
}

void Callable::marker () const {
    mo.marker();
    code.marker();
    mark(pos);
    mark(kw);
}

auto BoundMeth::call (ArgVec const& args) const -> Value {
    assert(args.num > 0 && this == &args[-1].obj());
    args[-1] = self; // overwrites the entry before first arg
    return meth.call({args.vec, (int) args.num + 1, (int) args.off - 1});
}

Closure::Closure (Callable const& f, ArgVec const& args)
        : func (f) {
    insert(0, args.num);
    for (int i = 0; i < args.num; ++i)
        begin()[i] = args[i];
}

auto Closure::call (ArgVec const& args) const -> Value {
    int n = size();
    assert(n > 0);
    Vector v;
    v.insert(0, n + args.num);
    for (int i = 0; i < n; ++i)
        v[i] = begin()[i];
    for (int i = 0; i < args.num; ++i)
        v[n+i] = args[i];
    return func.call({v, n + args.num, 0});
}

auto Context::taskPos () const -> int {
    for (auto e : Interp::tasks)
        if (e == this)
            return &e - Interp::tasks.begin();
    return -1;
}

void Context::enter (Callable const& func) {
    auto frameSize = func.code.fastSlotTop() + EXC_STEP * func.code.excLevel();
    int need = (frame().stack + frameSize) - (begin() + base);

    auto curr = base;           // current frame offset
    base = fill;                // new frame offset
    insert(fill, need);         // make room

    auto& f = frame();          // new frame
    f.base = curr;              // index of (now previous) frame
    f.spOff = spOff;            // previous stack index
    f.ipOff = ipOff;            // previous instruction index
    f.callee = callee;          // previous callable

    spOff = f.stack-begin()-1;  // stack starts out empty
    ipOff = 0;                  // code starts at first opcode
    callee = &func;             // new callable context
    f.ep = 0;                   // no exceptions pending
}

Value Context::leave (Value v) {
    auto& f = frame();
    auto r = f.result;          // stored result
    if (r.isNil())              // use return result if set
        r = v;                  // ... else arg

    if (base > NumSlots) {
        int prev = f.base;      // previous frame offset
        spOff = f.spOff;        // restore stack index
        ipOff = f.ipOff;        // restore instruction index
        callee = &f.callee.asType<Callable>(); // restore callee

        assert(fill > base);
        remove(base, fill - base); // delete current frame

        assert(prev >= 0);
        base = prev;            // new lower frame offset
    } else {
        // last frame, drop context, restore caller
        Interp::context = caller().ifType<Context>();
        if (this == &Interp::tasks[0].obj())
            Interp::tasks.pop(0);

        // FIXME ...
        remove(NumSlots, fill - NumSlots); // delete last frame
    }

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
    if (exc.isInt())
        num = exc;              // trigger soft-irq 1..31 (interrupt-safe)
    else
        event() = exc;      // trigger exception or other outer-loop req

    Interp::interrupt(num);     // set pending, to force inner loop exit
}

void Context::caught () {
    auto e = event();
    if (e.isNil())
        return; // there was no exception, just an inner loop exit
    event() = {};

    assert(frame().ep > 0); // simple exception, no stack unwind
    auto ep = excBase(0);
    ipOff = ep[0] & (FinallyTag - 1);
    spOff = ep[1];
    begin()[++spOff] = e.isNil() ? ep[3] : e;
}

auto Context::next () -> Value {
    Interp::resume(*this);
    return {}; // no result yet
}

void Context::marker () const {
    List::marker();
    mark(callee);
}

auto Interp::getQueueId () -> uint32_t {
    static_assert (MAX_QUEUES <= 8 * sizeof pending, "MAX_QUEUES too large");

    for (uint32_t id = 1; id < MAX_QUEUES; ++id) {
        auto mask = 1U << id;
        if ((queueIds & mask) == 0) {
            queueIds |= mask;
            return id;
        }
    }
    return 0;
}

void Interp::dropQueueId (uint32_t id) {
    auto mask = 1U << id;
    assert(queueIds & mask);
    queueIds &= ~mask;
    // TODO deal with tasks pending on this event
}

bool Interp::isAlive () {
    if (context != nullptr || pending != 0 || tasks.len() > 0)
        return true;
    for (auto e : tasks)
        if (!e.isNil())
            return true;
    return false;
}

void Interp::markAll () {
    //printf("\tgc started ...\n");
    mark(context);
    tasks.marker();
}

void Interp::snooze (uint32_t id, int ms, uint32_t flags) {
    assert(id < MAX_QUEUES);
    assert(queueIds & (1U << id));
    deadlines[id] = archTime() + ms;
    idleFlags[id] = flags;
}

void Interp::suspend (uint32_t id, Value t) {
    assert(id < MAX_QUEUES);

    if (t.isNil()) {
        assert(context != nullptr);
        t = context;
        context = context->caller().ifType<Context>();
    }

    t.asType<Context>().qid = id;
}

void Interp::resume (Context& ctx) {
    assert(context != &ctx);
    assert(ctx.caller().isNil());
    ctx.caller() = context;
    context = &ctx;
    interrupt(0); // force inner loop exit
}

void Interp::exception (Value v) {
    assert(!v.isInt());
    assert(context != nullptr);
    context->raise(v);
}

void Interp::interrupt (uint32_t num) {
    assert(num < MAX_QUEUES);
    // see https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
    __atomic_fetch_or(&pending, 1U << num, __ATOMIC_RELAXED);
}

auto Interp::nextPending () -> int {
    if (pending != 0)
        for (size_t num = 0; num < MAX_QUEUES; ++num)
            if (pendingBit(num))
                return num;
    return -1;
}

auto Interp::pendingBit (uint32_t num) -> bool {
    assert(num < MAX_QUEUES);
    auto mask = 1U << num;
    // see https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
    return (__atomic_fetch_and(&pending, ~mask, __ATOMIC_RELAXED) & mask) != 0;
}
