// stack.cpp - events, stacklets, and various call mechanisms

#include "monty.h"
#include <cassert>
#include <csetjmp>

#if NATIVE
extern void timerHook ();
#define INNER_HOOK  { timerHook(); }
#else
#define INNER_HOOK
#endif

using namespace monty;

List Context::ready;
uint32_t volatile Stacklet::pending;
Context* Context::current;

int Event::queued;
Vector Event::triggers;

static jmp_buf* resumer;

#if 0
struct Stacker : boss::Device {
    void process () override {
        if (gcCheck())
            Context::gcAll();
    }
};

Stacker stacker;
#endif

void Context::gcAll () {
    // careful to avoid infinite recursion: the "sys" module has "modules" as
    // one of its attributes, which is "Module::loaded", i.e. a dict which
    // chains to the built-in modules (see "qstr.cpp"), which includes "sys",
    // which has "modules" as one of its attributes, and on, and on, and on ...

    // this turns into infinite recursion because all these objects are static,
    // so there is no mark bit to tell the marker "been here before, skip me",
    // and the solution is simple: break this specific chain while marking

    auto save = Module::loaded._chain;
    Module::loaded._chain = nullptr;

    markVec(Event::triggers);
    mark(current);
    ready.marker();
    save->marker();

    // restore the broken chain, now that marking is complete
    Module::loaded._chain = save;

    sweep();
    compact();
}

#if 0
static void duff (void* dst, void const* src, size_t len) {
    //assert(((uintptr_t) dst & 3) == 0);
    //assert(((uintptr_t) src & 3) == 0);
    //assert((len & 3) == 0);
#if 0
    memcpy(dst, src, len);
#else
    // see https://en.wikipedia.org/wiki/Duff%27s_device
    auto to = (uint32_t*) dst;
    auto from = (uint32_t const*) src;
    auto count = len/4;
    auto n = (count + 7) / 8;
    switch (count % 8) {
        case 0: do { *to++ = *from++; [[fallthrough]];
        case 7:      *to++ = *from++; [[fallthrough]];
        case 6:      *to++ = *from++; [[fallthrough]];
        case 5:      *to++ = *from++; [[fallthrough]];
        case 4:      *to++ = *from++; [[fallthrough]];
        case 3:      *to++ = *from++; [[fallthrough]];
        case 2:      *to++ = *from++; [[fallthrough]];
        case 1:      *to++ = *from++;
                } while (--n > 0);
    }
#endif
}
#endif

auto Event::regHandler () -> uint32_t {
    _id = triggers.find({});
    if (_id >= (int) triggers.size())
        triggers.insert(_id);
    triggers[_id] = this;
    return _id;
}

void Event::deregHandler () {
    if (_id >= 0) {
        assert(&triggers[_id].obj() == this);
        triggers[_id] = {};
        set(); // release queued tasks
        _id = -1;
    }
}

auto Event::set () -> Value {
    _value = true;
    auto n = _queue.size();
    if (n > 0) {
        // insert all entries at head of ready and remove them from this event
        Context::ready.insert(0, n);
        memcpy(Context::ready.begin(), _queue.begin(), n * sizeof (Value));
        if (_id >= 0)
            queued -= n;
        assert(queued >= 0);
        _queue.clear();
    }
    return {};
}

auto Event::wait (int ms) -> Value {
    if (_value)
        return {};
    if (_id >= 0)
        ++queued;

    uint16_t remain = _deadline - nowAsTicks(); // must be modulo 16-bit!
    if (remain <= 60000 && ms < remain)
        _deadline -= remain - ms; // deadline must happen sooner

    assert(Context::current != nullptr);
    //FIXME? return Context::current->suspend(_queue, ms);
    _queue.append(Context::current);
    Context::current = nullptr;
    Context::setPending(0);
    return {};
}

// Timeouts use deadlines instead of down counters, as this avoids having to
// constantly adjust N counters on each tick. The logic is done modulo 65536,
// because 16-bit arithmetic is so easy to do: just use a uint16_t.
//
// With deadlines, the remaining time is always: (deadline - now) % 65536
// As the maximum valid timeout is 60s, this leads to the following ranges:
//
//      0 .. 60000 = ms ticks remaining before this timeout expires
//  60001 .. 65535 = the deadline has passed, but not yet dealt with
//
//  Each events will keep track of a "combined deadline", i.e. a deadline which
//  is guaranteed to be no later than the earliest one currently queued up. As
//  there's always a deadline, there may be "unused" triggers once every 60s.

auto Event::triggerExpired (uint32_t now) -> uint32_t {
    auto n = _queue.size();
    if (n == 0) // nothing queued
        return 0;
    uint16_t next = _deadline - now; // must be modulo 16-bit!
    if (next == 0 || next > 60000) {
        next = 60000;
        while (n-- > 0) { // loop from the end because queue items may get deleted
            auto& task = _queue[n].asType<Context>();
            uint16_t remain = task._transfer - now; // must be modulo 16-bit!
            if (remain == 0 || remain > 60000) { // can exceed deadline by max ≈ 5s
                --queued;
                _queue.remove(n);
                Context::ready.append(task);
                //task.timedOut(*this);
            } else if (next > remain)
                next = remain;
        }
        _deadline = now + next;
    }
    assert(next > 0);
    return next;
}

auto Event::unop ([[maybe_unused]] UnOp op) const -> Value {
    assert(op == UnOp::Boln);
    return Value::asBool(*this);
}

auto Event::create (ArgVec const& args, Type const*) -> Value {
    //CG: args
    return new Event;
}

void Event::repr (Buffer& buf) const {
    Object::repr(buf);
}

void Context::resumeCaller (Value v) {
    if (_caller != nullptr)
        _caller->_transfer = v;
    else if (v.isOk())
        v.dump("result lost"); // TODO just for debugging
    current = _caller;
    _caller = nullptr;
    setPending(0);
}

void Context::marker () const {
    mark(_caller);
    _transfer.marker();
    List::marker();
}

void Context::raise (Value v) {
    v.dump();
    assert(false); // TODO unhandled exception, can't continue
}

void Context::exception (Value e) {
    assert(current != nullptr);
    current->raise(e);
}

#if 0
void Stacklet::yield (bool fast) {
    if (fast) {
        if (pending == 0)
            return; // don't yield if there are no pending triggers
        assert(Context::ready.find(this) >= Context::ready.size());
        Context::ready.push(this);
        suspend();
    } else
        suspend(Context::ready);
}

// helper function to avoid stale register issues after setjmp return
auto stackletResumeFixer (void* p) -> Value {
    auto c = Context::current;
    duff(p, (jmp_buf*) c->end() + 1, (uint8_t*) resumer - (uint8_t*) p);
    return c->_transfer.take();
}

auto Stacklet::suspend (Vector& queue, int ms) -> Value {
    if (&queue != &Event::triggers) // special case: use as "do not append" mark
        queue.append(this);

    if (ms > 60000)
        ms = 60000;
    //FIXME _transfer = (uint16_t) (nowAsTicks() + ms);

    jmp_buf top;

    uint32_t need = (uint8_t*) resumer - (uint8_t*) &top;

    adj(_fill + need / sizeof (Value));
    //if (setjmp(top) == 0) {
    if (setjmp(*(jmp_buf*) end()) == 0) {
        // suspending: copy stack out and jump to caller
        //duff(end(), &top, need);
        duff((jmp_buf*) end() + 1, &top + 1, need - sizeof (jmp_buf));
        longjmp(*resumer, 1);
    }

    // resuming: copy stack back in
    return stackletResumeFixer(&top + 1);
}
#endif

auto Context::runLoop () -> bool {
    jmp_buf bottom;
    if (resumer == nullptr)
        resumer = &bottom;
    assert(resumer == &bottom); // make sure stack is always in same place

    setjmp(bottom); // suspend will always return here

    while (true) {
        INNER_HOOK

        auto flags = clearAllPending();
        for (auto e : Event::triggers)
            if (flags != 0) {
                if ((flags & 1) && e.isObj())
                    e->next();
                flags >>= 1;
            }

        current = (Context*) &ready.pull().obj();
        if (current == nullptr)
            break;

        if (current->cap() > current->_fill + sizeof (jmp_buf) / sizeof (Value))
            longjmp(*(jmp_buf*) current->end(), 1);

        while (current->run() && pending == 0) {}

        assert(ready.find(current) >= ready.size());
        if (current != nullptr)
            ready.push(current);
    }

    return Event::queued > 0 || ready.size() > 0;
}

void Module::repr (Buffer& buf) const {
    buf.print("<module '%s'>", (char const*) _name);
}

auto Function::call (ArgVec const& args) const -> Value {
    return _desc != nullptr ? parse(args) : _func(args);
}

auto Function::parse (ArgVec const& args) const -> Value {
    auto optional = false;
    union {
        int i; Object* o; char const* s; void* v;
    } params [10]; // max 9 actual parameters, in case ArgVec is inserted
    int pos = 0;
    auto desc = _desc;

    while (*desc != '*' && *desc != 0) {
        if (*desc == '?')
            optional = true; // following args are optional
        else {
            auto p = &params[pos+1];
            if (pos < args.size()) {
                auto a = args[pos];
                assert(a.isOk()); // args can't be nil ("None" is fine)
                switch (*desc) {
                    case 'i': { p->i = a; break; }
                    case 'o': { p->o = &a.asObj(); break; }
                    case 's': { p->s = a; break; }
                    case 'v': { *(Value*)p = a; break; }
                    default:  assert(false);
                }
            } else if (optional)
                p->v = nullptr;
            else
                return {E::TypeError, "need more args", pos};
            ++pos;
        }
        ++desc;
    }

    int remain = args.size() - pos;
    if (remain > 0 && *desc != '*')
        return {E::TypeError, "too many args", remain};

    auto ap = params + 1;
    if (optional || *desc == '*') {
        params[0].v = (void*) &args; // insert ArgVec as first actual parameter
        --ap;
        ++pos;
    }

    // FFI'ish call: pass parameters as if they were pointer-sized
    if (pos <= 4) { // up to 4 args uses only registers on ARM, more efficient
        using F4 = Value(*)(void*,void*,void*,void*);
        return ((F4) _func)(ap[0].v, ap[1].v, ap[2].v, ap[3].v);
    } else {
        using F10 = Value(*)(void*,void*,void*,void*,void*,
                             void*,void*,void*,void*,void*);
        return ((F10) _func)(ap[0].v, ap[1].v, ap[2].v, ap[3].v, ap[4].v,
                             ap[5].v, ap[6].v, ap[7].v, ap[8].v, ap[9].v);
    }
}
