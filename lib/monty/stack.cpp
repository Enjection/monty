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

List Stacklet::ready;
uint32_t volatile Stacklet::pending;
Stacklet* Stacklet::current;

int Event::queued;
Vector Event::triggers;

static jmp_buf* resumer;

void Stacklet::gcAll () {

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

static void duff (void* dst, void const* src, size_t len) {
    assert(((uintptr_t) dst & 3) == 0);
    assert(((uintptr_t) src & 3) == 0);
    assert((len & 3) == 0);
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
        Stacklet::ready.insert(0, n);
        memcpy(Stacklet::ready.begin(), _queue.begin(), n * sizeof (Value));
        if (_id >= 0)
            queued -= n;
        assert(queued >= 0);
        _queue.clear();
    }
    return {};
}

auto Event::wait () -> Value {
    if (!_value) {
        if (_id >= 0)
            ++queued;
        Stacklet::suspend(_queue);
    }
    return {};
}

auto Event::regHandler () -> uint32_t {
    _id = triggers.find({});
    if (_id >= (int) triggers.size())
        triggers.insert(_id);
    triggers[_id] = this;
    return _id;
}

auto Event::unop (UnOp op) const -> Value {
    assert(op == UnOp::Boln);
    return Value::asBool(*this);
}

auto Event::binop (BinOp op, Value rhs) const -> Value {
    assert(op == BinOp::Equal);
    return Value::asBool(rhs.isObj() && this == &rhs.obj());
}

auto Event::create (ArgVec const& args, Type const*) -> Value {
    //CG: args
    return new Event;
}

void Event::repr (Buffer& buf) const {
    Object::repr(buf);
}

void Stacklet::resumeCaller (Value v) {
    if (_caller != nullptr)
        _caller->_transfer = v;
    else if (v.isOk())
        v.dump("result lost"); // TODO just for debugging
    current = _caller;
    _caller = nullptr;
    setPending(0);
}

void Stacklet::marker () const {
    mark(_caller);
    _transfer.marker();
    List::marker();
}

void Stacklet::raise (Value v) {
    v.dump();
    assert(false); // TODO unhandled exception, can't continue
}

void Stacklet::exception (Value e) {
    assert(current != nullptr);
    current->raise(e);
}

void Stacklet::yield (bool fast) {
    assert(current != nullptr);
    if (fast) {
        if (pending == 0)
            return; // don't yield if there are no pending triggers
        assert(ready.find(current) >= ready.size());
        ready.push(current);
        // TODO ????? current = nullptr;
        suspend();
    } else
        suspend(ready);
}

// helper function to avoid stale register issues after setjmp return
static auto resumeFixer (void* p) -> Value {
    auto c = Stacklet::current;
    assert(c != nullptr);
    assert(resumer != nullptr);
    auto n = (uint8_t*) resumer - (uint8_t*) p;
    assert((n & 3) == 0);
    duff(p, c->end(), n);
    return c->_transfer.take();
}

auto Stacklet::suspend (Vector& queue) -> Value {
    assert(current != nullptr);
    if (&queue != &Event::triggers) // special case: use as "do not append" mark
        queue.append(current);

    jmp_buf top;
    assert(resumer > &top);

    uint32_t need = (uint8_t*) resumer - (uint8_t*) &top;
    assert(need % sizeof (Value) == 0);
    assert(need > sizeof (jmp_buf));

    current->adj(current->_fill + need / sizeof (Value));
    if (setjmp(top) == 0) {
        // suspending: copy stack out and jump to caller
        duff(current->end(), &top, need);
        longjmp(*resumer, 1);
    }

    // resuming: copy stack back in
    return resumeFixer(&top);
}

auto Stacklet::runLoop () -> bool {
    jmp_buf bottom;
    if (resumer == nullptr)
        resumer = &bottom;
    assert(resumer == &bottom); // make sure stack is always in same place

    setjmp(bottom); // suspend will always return here

    while (true) {
        INNER_HOOK

        auto flags = allPending();
        for (auto& e : Event::triggers)
            if (flags != 0) {
                if ((flags & 1) && e.isObj())
                    e.asType<Event>().set();
                flags >>= 1;
            }

        current = (Stacklet*) &ready.pull().obj();
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
