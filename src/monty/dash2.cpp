// FIXME was: type.cpp - collection types and type system

#include <cstdint>
#include <cstring>

#include "vecs.h"
#include "objs.h"
#include "dash.h"

#include <cassert>
#include <cstdarg>

extern "C" int printf (char const*, ...);

using namespace monty;

Tuple const Tuple::emptyObj;
Value const monty::Empty {Tuple::emptyObj};

Type Inst::info (Q(0,"<instance>"));

Lookup const Bytes::attrs;
Lookup const Class::attrs;
Lookup const Inst::attrs;
Lookup const Set::attrs;
Lookup const Str::attrs;
Lookup const Super::attrs;
Lookup const Tuple::attrs;
Lookup const Type::attrs;

void monty::markVec (Vector const& vec) {
    for (auto e : vec)
        e.marker();
}

auto ArgVec::parse (char const* desc, ...) const -> Value {
    auto args = begin();
    auto optSkip = 0;

    va_list ap;
    va_start(ap, desc);
    for (int i = 0; desc[i] != 0; ++i) {
        switch (desc[i]) {
            case '?': optSkip = 1; continue; // following args are optional
            case '*': return end() - args;   // extra args are ok
        }

        if (i-optSkip >= size()) { // ran out of values
            if (optSkip == 0)
                return {E::TypeError, "need more args", i};

            switch (desc[i]) { // set to default
                case 'i': { auto p = va_arg(ap, int*); *p = 0; break; }
                case 'o': { auto p = va_arg(ap, Object**); *p = nullptr; break; }
                case 's': { auto p = va_arg(ap, char const**); *p = nullptr; break; }
                case 'v': { auto p = va_arg(ap, Value*); *p = {}; break; }
                default:  assert(false);
            }
        } else {
            auto a = *args;
            assert(a.isOk()); // args can't be nil ("None" is fine)
            switch (desc[i]) {
                case 'i': { auto p = va_arg(ap, int*); *p = a; break; }
                case 'o': { auto p = va_arg(ap, Object**); *p = &a.asObj(); break; }
                case 's': { auto p = va_arg(ap, char const**); *p = a; break; }
                case 'v': { auto p = va_arg(ap, Value*); *p = a; break; }
                default:  assert(false);
            }
        }

        ++args;
    }
    va_end(ap);

    if (args < end())
        return {E::TypeError, "too many args", (int) (end()-args)};

    return end() - args;
}

static void putcEsc (Buffer& buf, char const* fmt, uint8_t ch) {
    buf.putc('\\');
    switch (ch) {
        case '\t': buf.putc('t'); break;
        case '\n': buf.putc('n'); break;
        case '\r': buf.putc('r'); break;
        default:   buf.print(fmt, ch); break;
    }
}

static void putsEsc (Buffer& buf, Value s) {
    buf.putc('"');
    for (auto p = (uint8_t const*) (char const*) s; *p != 0; ++p) {
        if (*p == '\\' || *p == '"')
            buf.putc('\\');
        if (*p >= ' ')
            buf.putc(*p);
        else
            putcEsc(buf, "u%04x", *p);
    }
    buf.putc('"');
}

Buffer::~Buffer () {
    for (uint32_t i = 0; i < _fill; ++i)
        printf("%c", begin()[i]); // TODO yuck
}

void Buffer::write (uint8_t const* ptr, uint32_t len) {
    if (_fill + len > cap())
        adj(_fill + len + 50);
    memcpy(end(), ptr, len);
    _fill += len;
}

auto Buffer::operator<< (Value v) -> Buffer& {
    switch (v.tag()) {
        case Value::Nil: print("_"); break;
        case Value::Int: print("%d", (int) v); break;
        case Value::Str: putsEsc(*this, v); break;
        case Value::Obj: v->repr(*this); break;
    }
    return *this;
}

// formatted output, adapted from JeeH

auto Buffer::splitInt (uint32_t val, int base, uint8_t* buf) -> int {
    int i = 0;
    do {
        buf[i++] = val % base;
        val /= base;
    } while (val != 0);
    return i;
}

void Buffer::putFiller (int n, char fill) {
    while (--n >= 0)
        putc(fill);
}

void Buffer::putInt (int val, int base, int width, char fill) {
    uint8_t buf [33];
    int n;
    if (val < 0 && base == 10) {
        n = splitInt(-val, base, buf);
        if (fill != ' ')
            putc('-');
        putFiller(width - n - 1, fill);
        if (fill == ' ')
            putc('-');
    } else {
        n = splitInt(val, base, buf);
        putFiller(width - n, fill);
    }
    while (n > 0) {
        uint8_t b = buf[--n];
        putc("0123456789ABCDEF"[b]);
    }
}

void Buffer::print (char const* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    while (*fmt)
        if (char c = *fmt++; c == '%') {
            char fill = *fmt == '0' ? '0' : ' ';
            int width = 0, base = 0;
            while (base == 0) {
                c = *fmt++;
                switch (c) {
                    case 'b':
                        base =  2;
                        break;
                    case 'o':
                        base =  8;
                        break;
                    case 'd':
                        base = 10;
                        break;
                    case 'p':
                        fill = '0';
                        width = 8;
                        [[fallthrough]];
                    case 'x':
                        base = 16;
                        break;
                    case 'c':
                        putFiller(width - 1, fill);
                        c = va_arg(ap, int);
                        [[fallthrough]];
                    case '%':
                        putc(c);
                        base = 1;
                        break;
                    case 's': {
                        auto s = va_arg(ap, char const*);
                        while (*s) {
                            putc(*s++);
                            --width;
                        }
                        putFiller(width, fill);
                        [[fallthrough]];
                    }
                    default:
                        if ('0' <= c && c <= '9')
                            width = 10 * width + c - '0';
                        else
                            base = 1; // stop scanning
                }
            }
            if (base > 1) {
                int val = va_arg(ap, int);
                putInt(val, base, width, fill);
            }
        } else
            putc(c);
    va_end(ap);
}

Bytes::Bytes (void const* ptr, uint32_t len) {
    insert(0, len);
    if (ptr != nullptr)
        memcpy(begin(), ptr, len);
    // else cleared by insert
}

auto Bytes::unop (UnOp op) const -> Value {
    switch (op) {
        case UnOp::Boln: return Value::asBool(size());
        case UnOp::Hash: return Q::hash(begin(), size());
        default:         break;
    }
    return Object::unop(op);
}

auto Bytes::binop (BinOp op, Value rhs) const -> Value {
    auto& val = rhs.asType<Bytes>();
    assert(size() == val.size());
    switch (op) {
        case BinOp::Equal:
            return Value::asBool(size() == val.size() &&
                                    memcmp(begin(), val.begin(), size()) == 0);
        default:
            return Object::binop(op, rhs);
    }
}

auto Bytes::getAt (Value k) const -> Value {
    return k.isInt() ? (Value) (*this)[k] : sliceGetter(k);
}

auto Bytes::copy (Range const& r) const -> Value {
    auto n = r.len();
    auto v = new Bytes;
    v->insert(0, n);
    for (uint32_t i = 0; i < n; ++i)
        (*v)[i] = (*this)[r.getAt(i)];
    return v;
}

auto Bytes::create (ArgVec const& args, Type const*) -> Value {
    //CG: args v
    const void* p = nullptr;
    uint32_t n = 0;
    if (v.isInt())
        n = v;
    else if (v.isStr()) {
        p = (char const*) v;
        n = strlen((char const*) p);
    } else if (auto ps = v.ifType<Str>(); ps != 0) {
        p = ps->begin();
        n = ps->size();
    } else if (auto pb = v.ifType<Bytes>(); pb != 0) {
        p = pb->begin();
        n = pb->size();
    } else
        assert(false); // TODO iterables
    return new Bytes (p, n);
}

void Bytes::repr (Buffer& buf) const {
    buf << '\'';
    for (auto b : *this) {
        if (b == '\\' || b == '\'')
            buf << '\\';
        if (b >= ' ')
            buf.putc(b);
        else
            putcEsc(buf, "x%02x", b);
    }
    buf << '\'';
}

Str::Str (char const* s, int n)
        : Bytes (s, n >= 0 ? n : s != nullptr ? strlen(s) : 0) {
    adj(_fill+1);
    *end() = 0;
}

auto Str::unop (UnOp op) const -> Value {
    if (op == UnOp::Intg)
        return Int::conv((char const*) begin());
    return Bytes::unop(op);
}

auto Str::binop (BinOp op, Value rhs) const -> Value {
    switch (op) {
        case BinOp::Equal:
            return rhs == *this;
        case BinOp::Add: {
            auto l = (char const*) begin();
            char const* r = rhs.isStr() ? rhs : rhs.asType<Str>();
            int nl = size(), nr = strlen(r);
            auto o = new Str (nullptr, nl + nr);
            memcpy((char*) o->begin(), l, nl);
            memcpy((char*) o->begin() + nl, r, nr);
            return o;
        }
        default:
            return Bytes::binop(op, rhs);
    }
}

auto Str::getAt (Value k) const -> Value {
    if (!k.isInt())
        return sliceGetter(k);
    int idx = k;
    if (idx < 0)
        idx += size();
    return new Str ((char const*) begin() + idx, 1); // TODO utf-8
}

auto Str::create (ArgVec const& args, Type const*) -> Value {
    //CG: args arg:s
    return new Str (arg);
}

void Str::repr (Buffer& buf) const {
    putsEsc(buf, (char const*) begin());
}

void VaryVec::atAdj (uint32_t idx, uint32_t len) {
    assert(idx < _fill);
    auto olen = atLen(idx);
    if (len == olen)
        return;
    auto ofill = _fill;
    _fill = pos(_fill);
    if (len > olen)
        ByteVec::insert(pos(idx+1), len - olen);
    else
        ByteVec::remove(pos(idx) + len, olen - len);
    _fill = ofill;

    for (uint32_t i = idx + 1; i <= _fill; ++i)
        pos(i) += len - olen;
}

void VaryVec::atSet (uint32_t idx, void const* ptr, uint32_t len) {
    atAdj(idx, len);
    memcpy(begin() + pos(idx), ptr, len);
}

void VaryVec::insert (uint32_t idx, uint32_t num) {
    assert(idx <= _fill);
    if (cap() == 0) {
        ByteVec::insert(0, 2);
        pos(0) = 2;
        _fill = 0;
    }

    auto ofill = _fill;
    _fill = pos(_fill);
    ByteVec::insert(2 * idx, 2 * num);
    _fill = ofill + num;

    for (uint32_t i = 0; i <= _fill; ++i)
        pos(i) += 2 * num;
    for (uint32_t i = 0; i < num; ++i)
        pos(idx+i) = pos(idx+num);
}

void VaryVec::remove (uint32_t idx, uint32_t num) {
    assert(idx + num <= _fill);
    auto diff = pos(idx+num) - pos(idx);

    auto ofill = _fill;
    _fill = pos(_fill);
    ByteVec::remove(pos(idx), diff);
    ByteVec::remove(2 * idx, 2 * num);
    _fill = ofill - num;

    for (uint32_t i = 0; i <= _fill; ++i)
        pos(i) -= 2 * num;
    for (uint32_t i = idx; i <= _fill; ++i)
        pos(i) -= diff;
}

auto Lookup::operator[] (Q key) const -> Value {
    assert(key._id > 0);
    for (uint32_t i = 0; i < _count; ++i)
        if (key._id == _items[i].k._id)
            return _items[i].v;
    return _chain != nullptr ?  (*_chain)[key] : Value {};
}

auto Lookup::getAt (Value k) const -> Value {
    assert(k.isQid());
    return (*this)[k.asQid()];
}

auto Lookup::attrDir (Value v) const -> Lookup const* {
    uint32_t i = 0;
    if (this == Module::builtins._chain) // are these the built-in attributes?
        i = (int) E::UnicodeError + 1;   // ... then skip all the exceptions
    while (i < _count)
        v->setAt(_items[i++].k, true);
    return _chain;
}

Tuple::Tuple (Value seq) {
    for (auto e : seq)
        append(e);
}

Tuple::Tuple (ArgVec const& args) {
    insert(0, args.size());
    for (int i = 0; i < args.size(); ++i)
        (*this)[i] = args[i];
}

auto Tuple::getAt (Value k) const -> Value {
    if (!k.isInt())
        return sliceGetter(k);
    auto n = relPos(k);
    assert(n < size());
    return (*this)[n];
}

auto Tuple::copy (Range const& r) const -> Value {
    // TODO can't use this, because of the range: return type().call(*this);
    //  the proper solution is to support arbitrary iterators & generators
    auto& v = &type() == &info ? *new Tuple : *new List;
    auto n = r.len();
    v.insert(0, n);
    for (uint32_t i = 0; i < n; ++i)
        v[i] = (*this)[r.getAt(i)];
    return v;
}

auto Tuple::create (ArgVec const& args, Type const*) -> Value {
    //CG: args ? arg
    return new Tuple (arg);
}

void Tuple::printer (Buffer& buf, char const* sep) const {
    buf << sep[0];
    for (uint32_t i = 0; i < _fill; ++i) {
        if (i > 0)
            buf << ',';
        buf << (*this)[i];
        if (sep[2] != 0)
            buf << sep[2] << (*this)[_fill+i]; // special-cased for Dict
    }
    buf << sep[1];
}

void Tuple::repr (Buffer& buf) const {
    printer(buf, "()");
}

auto List::pop (int idx) -> Value {
    auto n = relPos(idx);
    assert(size() > n);
    Value v = (*this)[n];
    remove(n);
    return v;
}

auto List::append (Value v) -> Value {
    Vector::append(v);
    return {};
}

auto List::setAt (Value k, Value v) -> Value {
    if (!k.isInt())
        return sliceSetter(k, v);
    auto n = relPos(k);
    assert(n < size());
    return (*this)[n] = v;
}

auto List::store (Range const& r, Object const& v) -> Value {
    assert(r._by == 1);
    int olen = r.len();
    int nlen = v.len();
    if (nlen < olen)
        remove(r._from + nlen, olen - nlen);
    else if (nlen > olen)
        insert(r._from + olen, nlen - olen);
    for (int i = 0; i < nlen; ++i)
        (*this)[r.getAt(i)] = v.getAt(i);
    return {};
}

auto List::create (ArgVec const& args, Type const*) -> Value {
    //CG: args ? arg
    return new List (arg);
}

void List::repr (Buffer& buf) const {
    printer(buf, "[]");
}

auto Set::find (Value v) const -> uint32_t {
    for (auto& e : *this)
        if (v == e)
            return &e - begin();
    return size();
}

auto Set::Proxy::operator= (bool f) -> bool {
    auto n = s.size();
    auto pos = s.find(v);
    if (pos < n && !f)
        s.remove(pos);
    else if (pos == n && f) {
        s.insert(pos);
        s[pos] = v;
    }
    return pos < n;
}

auto Set::binop (BinOp op, Value rhs) const -> Value {
    if (op == BinOp::Contains)
        return Value::asBool(find(rhs) < size());
    return Object::binop(op, rhs);
}

auto Set::getAt (Value k) const -> Value {
    return Value::asBool(has(k));
}

auto Set::setAt (Value k, Value v) -> Value {
    auto f = has(k) = v.truthy();
    return Value::asBool(f);
}

auto Set::create (ArgVec const& args, Type const*) -> Value {
    //CG: args ? arg
    return new Set (arg);
}

void Set::repr (Buffer& buf) const {
    printer(buf, "{}");
}

// was: CG3 type <dictview>
struct DictView : Object {
    static Type info;
    auto type () const -> Type const& override { return info; }

    DictView (Dict const& dict, int vtype) : _dict (dict), _vtype (vtype) {}

    auto len () const -> uint32_t override { return _dict._fill; }
    auto getAt (Value k) const -> Value override;
    auto iter () const -> Value override { return 0; }

    void marker () const override { _dict.marker(); }
private:
    Dict const& _dict;
    int _vtype;
};

Type DictView::info (Q(0,"<dictview>"));

// dict invariant: items layout is: N keys, then N values, with N == d.size()
auto Dict::Proxy::operator= (Value v) -> Value {
    Value w;
    auto n = d.size();
    auto pos = d.find(k);
    if (v.isNil()) {
        if (pos < n) {
            d._fill = 2*n;    // don't wipe existing vals
            d.remove(n+pos);  // remove value
            d.remove(pos);    // remove key
            d._fill = --n;    // set length to new key count
        }
    } else {
        if (pos == n) { // move all values up and create new gaps
            d._fill = 2*n;    // don't wipe existing vals
            d.insert(2*n);    // create slot for new value
            d.insert(n);      // same for key, moves all vals one up
            d._fill = ++n;    // set length to new key count
            d[pos] = k;       // store the key
        } else
            w = d[n+pos];
        assert(d.cap() >= 2*n);
        d[n+pos] = v;
    }
    return w;
}

Dict::Dict (Value seq) {
    auto d = seq.ifType<Dict>();
    for (auto e : seq)
        if (d != nullptr)
            at(e) = (Value) d->at(e);
        else {
            auto& t = e.asType<Tuple>();
            at(t[0]) = t[1];
        }
}

auto Dict::at (Value k) const -> Value {
    auto n = size();
    auto pos = find(k);
    return pos < n ? (*this)[n+pos] :
            _chain != nullptr ? _chain->getAt(k) : Value {};
}

auto Dict::keys () -> Value {
    return new DictView (*this, 0);
}

auto Dict::values () -> Value {
    return new DictView (*this, 1);
}

auto Dict::items () -> Value {
    return new DictView (*this, 2);
}

void Dict::marker () const {
    auto& v = (Vector const&) *this;
    for (uint32_t i = 0; i < 2*_fill; ++i) // note: twice the fill
        v[i].marker();
    mark(_chain);
}

auto Dict::create (ArgVec const& args, Type const*) -> Value {
    auto d = new Dict (args.size() == 1 ? args[0] : Value {});
    if (args.size() % 2 == 0)
        for (int i = 0; i < args.kwNum(); ++i)
            d->at(args.kwKey(i)) = args.kwVal(i);
    return d;
}

void Dict::repr (Buffer& buf) const {
    printer(buf, "{}:");
}

auto DictView::getAt (Value k) const -> Value {
    assert(k.isInt());
    int n = k;
    if (_vtype == 1)
        n += _dict._fill;
    if (_vtype <= 1)
        return _dict[n];
    Value args [] = {_dict[n], _dict[n+_dict._fill]};
    return new Tuple ({args});
}

auto Type::noFactory (ArgVec const&, const Type*) -> Value {
    assert(false);
    return {};
}

auto Type::create (ArgVec const& args, Type const*) -> Value {
    //CG: args v
    switch (v.tag()) {
        case Value::Nil: break;
        case Value::Int: return Q(0,"int");
        case Value::Str: return Q(0,"str");
        case Value::Obj: return v->type()._name;
    }
    return {};
}

void Type::repr (Buffer& buf) const {
    buf.print("<type %s>", (char const*) _name);
}

Class::Class (ArgVec const& args) : Type (args[1], nullptr, Inst::create) {
    assert(2 <= args.size() && args.size() <= 3); // no support for multiple inheritance
    if (args.size() > 2)
        _chain = &args[2].asType<Class>();

    at(Q(0,"__name__")) = args[1];
    at(Q(0,"__bases__")) = new Tuple ({args._vec, args.size()-2, args._off+2});

    args[0]->call({args._vec, args.size() - 2, args._off + 2});
}

auto Class::create (ArgVec const& args, Type const*) -> Value {
    assert(args.size() >= 2 && args[0].isObj() && args[1].isStr());
    return new Class (args);
}

void Class::repr (Buffer& buf) const {
    buf.print("<class %s>", (char const*) at("__name__"));
}

auto Super::create (ArgVec const& args, Type const*) -> Value {
    //CG: args sclass sinst
    return new Super (sclass, sinst);
}

void Super::repr (Buffer& buf) const {
    buf << "<super: ...>";
}

Inst::Inst (ArgVec const& args, Class const& cls) : Dict (&cls) {
    Value self;
    Value init = attr(Q(0,"__init__"), self);
    if (init.isOk()) {
        // stuff "self" before the args passed in TODO is this always ok ???
        args[-1] = this;
        init->call({args._vec, args.size() + 1, args._off - 1});
    }
}

auto Inst::create (ArgVec const& args, Type const* t) -> Value {
    Value v = t;
    return new Inst (args, v.asType<Class>());
}

void Inst::repr (Buffer& buf) const {
    buf.print("<%s object at %p>", (char const*) type()._name, this);
}

#if DOCTEST
#include <doctest.h>
namespace {
#include "dash2-test.h"
}
#endif
