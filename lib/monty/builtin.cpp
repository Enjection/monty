// builtin.cpp - exceptions and auto-generated built-in tables

#include "monty.h"

//CG1 if dir extend
//#include <extend.h>

using namespace monty;

//CG1 bind argtest a1 a2 a3:o a4:i ? a5 a6:s a7:s a8 *
static auto f_argtest (ArgVec const& args, Value a1, Value a2, Object* a3, int a4, Value a5, char const* a6, char const* a7, Value a8) -> Value {
    //CG: kwargs foo bar baz
    if (a1.isInt()) // special, returns parse result: N<0 = missing, N>0 = extra
        return args.size();
    auto n = a1.id()+a2.id()+a5.id()+a8.id()+(int)(uintptr_t)a3+a4;
    return n + (a6 != nullptr ? *a6 : 0) + (a7 != nullptr ? *a7 : 0);
}

//CG1 bind print *
static auto f_print (ArgVec const& args) -> Value {
    //CG: kwargs sep end
    if (!sep.isStr())
        sep = " ";
    if (!end.isStr())
        end = "\n";
    Buffer buf;
    for (int i = 0; i < args.size(); ++i) {
        // TODO ugly logic to avoid quotes and escapes for string args
        //  this only applies to the top level, not inside lists, etc.
        char const* s = nullptr;
        Value v = args[i];
        if (v.isStr())
            s = v;
        else {
            auto p = v.ifType<Str>();
            if (p != nullptr)
                s = *p;
        }
        if (i > 0)
            buf.puts(sep);
        // if it's a plain string, print as is, else print via repr()
        if (s != nullptr)
            buf << s;
        else
            buf << v;
    }
    buf.puts(end);
    return {};
}

//CG1 bind iter obj:o
static auto f_iter (Object* obj) -> Value {
    auto v = obj->iter();
    return v.isObj() ? v : new Iterator (obj, 0);
}

//CG1 bind next arg
static auto f_next (Value arg) -> Value {
    auto v = arg->next();
    if (v.isNil() && Context::current != nullptr)
        return Value {E::StopIteration};
    return v;
}

//CG1 bind len arg
static auto f_len (Value arg) -> Value {
    return arg.isStr() ? strlen(arg) : arg->len();
}

//CG1 bind abs arg
static auto f_abs (Value arg) -> Value {
    return arg.unOp(UnOp::Abso);
}

//CG1 bind hash arg
static auto f_hash (Value arg) -> Value {
    return arg.unOp(UnOp::Hash);
}

//CG1 bind id arg
static auto f_id (Value arg) -> Value {
    return arg.id();
}

//CG1 bind dir arg
static auto f_dir (Value arg) -> Value {
    Object const* obj = &arg.asObj();
    if (obj != &Module::builtins &&
            obj != &Module::loaded &&
            &obj->type() != &Type::info &&
            &obj->type() != &Module::info)
        obj = &obj->type();

    auto r = new Set;
    do {
        if (obj != &Module::builtins && obj != &Module::loaded)
            for (auto e : *(Dict const*) obj)
                r->has(e) = true;
        //obj->type()._name.dump("switch");
        switch (obj->type()._name.asQid()) {
            case Q(0,"<module>"):
            case Q(0,"type")._id:
            case Q(0,"dict")._id:
                obj = ((Dict const*) obj)->_chain; break;
            case Q(0,"<lookup>"): 
                obj = ((Lookup const*) obj)->attrDir(r); break;
            default: obj = nullptr;
        }
    } while (obj != nullptr);
    return r;
}

//CG: exception-emit f

static Lookup::Item const exceptionMap [] = {
    //CG: exception-emit h
};

Lookup const Exception::bases (exceptionMap);

//CG: wrappers *

static Lookup::Item const builtinsMap [] = {
    // exceptions must be first in the map, see Exception::findId
    //CG: exception-emit d
    //CG: type-builtin
    //CG: builtins
};

static Lookup const builtins_attrs (builtinsMap);
Dict Module::builtins (&builtins_attrs);

Exception::Exception (E code, ArgVec const& args) : Tuple (args), _code (code) {
    adj(_fill+1);
    (*this)[_fill] = _fill+1;
    // TODO nasty, fixup Exception::info, as it doesn't have the attrs set
    //  because exception is not an exposed type ("<exception>" iso "exception")
    info._chain = &attrs;
}

auto Exception::findId (Function const& f) -> int {
    for (auto& e : builtinsMap)
        if (&f == &e.v.obj())
            return &e - builtinsMap;
    // searches too many entries, but the assumption is that f will be found
    return -1;
}

auto Exception::binop (BinOp op, Value rhs) const -> Value {
    if (op == BinOp::ExceptionMatch) {
        auto id = findId(rhs.asType<Function>());
        auto code = (int) _code;
        do {
            if (code == id)
                return True;
            code = exceptionMap[code].v;
        } while (code >= 0);
        return False;
    }
    return Tuple::binop(op, rhs);
}

struct SizeFix {
    SizeFix (Exception& exc) : _exc (exc), _orig (exc._fill) {
        _exc._fill = _exc[_orig]; // expose the trace info, temporarily
    }
    ~SizeFix () {
        _exc[_orig] = _exc._fill;
        _exc._fill = _orig;
    }

    Exception& _exc;
    uint32_t _orig;
};

void Exception::addTrace (uint32_t off, Value bc) {
    SizeFix fixer (*this);
    append(off);
    append(bc);
}

auto Exception::trace () -> Value {
    auto r = new List;
    SizeFix fixer (*this);
    for (uint32_t i = fixer._orig + 1; i < size(); i += 2) {
        auto e = (*this)[i+1]; // Bytecode object
        r->append(e->getAt(-1));            // filename
        r->append(e->getAt((*this)[i]));    // line number
        r->append(e->getAt(-2));            // function
    }
    return r;
}

void Exception::marker () const {
    SizeFix fixer (*const_cast<Exception*>(this));
    Tuple::marker();
}

auto Exception::create (E exc, ArgVec const& args) -> Value {
    return new Exception (exc, args);
}

void Exception::repr (Buffer& buf) const {
    buf << (char const*) bases._items[(int) _code].k;
    Tuple::repr(buf);
}

//CG: type-info

#if DOCTEST
#include <doctest.h>

TEST_CASE("struct sizes") {
    CHECK(sizeof (ByteVec) == sizeof (VaryVec));
    CHECK(sizeof (void*) == sizeof (Value));
    CHECK(sizeof (void*) == sizeof (Object));
    CHECK(sizeof (void*) == sizeof (None));
    CHECK(sizeof (void*) == sizeof (Bool));
    CHECK(4 * sizeof (void*) == sizeof (Iterator));
    CHECK(2 * sizeof (void*) + 8 == sizeof (Bytes));
    CHECK(2 * sizeof (void*) + 8 == sizeof (Str));
    CHECK(4 * sizeof (void*) == sizeof (Lookup));
    CHECK(2 * sizeof (void*) + 8 == sizeof (Tuple));
    CHECK(3 * sizeof (void*) + 8 == sizeof (Exception));

    // on 32b arch, packed = 12 bytes, which will fit in 2 GC slots i.s.o. 3
    // on 64b arch, packed is the same as unpacked as void* is also 8 bytes
    struct Packed { void* p; int64_t i; } __attribute__((packed));
    struct Normal { void* p; int64_t i; };
    CHECK((sizeof (Packed) < sizeof (Normal) || 8 * sizeof (void*) == 64));

    CHECK(sizeof (Packed) >= sizeof (Int));
    //CHECK(sizeof (Normal) <= sizeof (Int)); // fails on RasPi-32b

    // TODO incorrect formulas (size rounded up), but it works on 32b & 64b ...
    CHECK(2 * sizeof (void*) + 8 == sizeof (Range));
    CHECK(4 * sizeof (void*) == sizeof (Slice));

    CHECK(2*sizeof (uint32_t) + 2*sizeof (void*) == sizeof (Buffer));
}

#endif // DOCTEST
