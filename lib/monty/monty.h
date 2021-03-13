// Monty, a stackless VM - main header

#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" int printf (char const*, ...);

namespace monty {
    //CG1 version
    constexpr auto VERSION = "<stripped>";

// see gc.cpp - objects and vectors with garbage collection

    struct Obj {
        Obj () =default;
        virtual ~Obj () =default;

        virtual void marker () const {} // called to mark all ref'd objects

        static auto isInPool (void const* p) -> bool;
        auto isCollectable () const { return isInPool(this); }

        auto operator new (size_t bytes) -> void*;
        auto operator new (size_t bytes, uint32_t extra) -> void* {
            return operator new (bytes + extra);
        }
        void operator delete (void*);

        static void sweep ();   // reclaim all unmarked objects
        static void dumpAll (); // like sweep, but only to print all obj+free

        // JT's "Rule of 5"
        Obj (Obj&&) =delete;
        Obj (Obj const&) =delete;
        auto operator= (Obj&&) -> Obj& =delete;
        auto operator= (Obj const&) -> Obj& =delete;
    };

    void mark (Obj const&);
    inline void mark (Obj const* p) { if (p != nullptr) mark(*p); }

    struct Vec {
        constexpr Vec () =default;
        constexpr Vec (void const* data, uint32_t capa =0)
                    : _data ((uint8_t*) data), _capa (capa) {}
        ~Vec () { (void) adj(0); }

        static auto isInPool (void const* p) -> bool;
        auto isResizable () const { return _data == nullptr || isInPool(_data); }

        static void compact (); // reclaim and compact unused vector space
        static void dumpAll (); // like compact, but only to print all vec+free

    protected:
        auto ptr () const { return _data; }
        auto cap () const { return _capa; }
        auto adj (uint32_t bytes) -> bool;
    private:
        uint8_t* _data = nullptr; // pointer to vector when capa > 0
        uint32_t _capa = 0;       // capacity in bytes

        auto slots () const -> uint32_t; // capacity in vecslots
        auto findSpace (uint32_t) -> void*; // hidden private type

        // JT's "Rule of 5"
        Vec (Vec&&) =delete;
        Vec (Vec const&) =delete;
        auto operator= (Vec&&) -> Vec& =delete;
        auto operator= (Vec const&) -> Vec& =delete;
    };

    void gcSetup (void* base, uint32_t size); // configure the memory pool
    auto gcMax () -> int;    // return free space between objects & vectors
    auto gcCheck () -> bool; // true when it's time to collect the garbage
    void gcReport ();        // print a brief gc summary with statistics

    union GCStats {
        struct {
            int
                checks, sweeps, compacts,
                toa, tob, tva, tvb, // totalObjAllocs/Bytes,totalVecAllocs/Bytes
                coa, cob, cva, cvb, // currObjAllocs/Bytes,currVecAllocs/Bytes
                moa, mob, mva, mvb; // maxObjAllocs/Bytes,maxVecAllocs/Bytes
        };
        int v [15];
    };
    extern GCStats gcStats;

    extern void* (*panicOutOfMemory)(); // triggers an assertion by default

// see data.cpp - basic object data types

    // forward decl's
    struct Object;
    struct Lookup;
    struct Buffer;
    struct Type;
    struct Range;
    struct RawIter;

    extern char const qstrBase [];
    extern int const qstrBaseLen;
    void qstrCleanup ();

    struct Q {
        constexpr Q (uint16_t id, char const* =nullptr) : _id (id) {}

        operator char const* () const { return str(_id); }
        constexpr operator uint16_t () const { return _id; }

        static auto hash (void const*, int =-1) -> uint32_t;
        static auto str (uint16_t) -> char const*;
        static auto find (char const*) -> uint16_t;
        static auto make (char const*) -> uint16_t;
        static auto last () -> uint16_t;

        uint16_t _id;
    };

    enum class E : uint8_t { // parsed by codegen.py, see builtin.cpp
        //CG< exceptions
        BaseException,       // -
        Exception,           // BaseException
        StopIteration,       // Exception
        AssertionError,      // Exception
        AttributeError,      // Exception
        EOFError,            // Exception
        ImportError,         // Exception
        MemoryError,         // Exception
        NameError,           // Exception
        OSError,             // Exception
        TypeError,           // Exception
        ArithmeticError,     // Exception
        ZeroDivisionError,   // ArithmeticError
        LookupError,         // Exception
        IndexError,          // LookupError
        KeyError,            // LookupError
        RuntimeError,        // Exception
        NotImplementedError, // RuntimeError
        ValueError,          // Exception
        UnicodeError,        // ValueError
        //CG>
    };

    enum UnOp : uint8_t { Pos, Neg, Inv, Not, Boln, Hash, Abso, Intg, };

    enum BinOp : uint8_t {
        //CG< binops ../../git/micropython/py/runtime0.h 35
        Less, More, Equal, LessEqual, MoreEqual, NotEqual, In, Is,
        ExceptionMatch, InplaceOr, InplaceXor, InplaceAnd, InplaceLshift,
        InplaceRshift, InplaceAdd, InplaceSubtract, InplaceMultiply,
        InplaceMatMultiply, InplaceFloorDivide, InplaceTrueDivide,
        InplaceModulo, InplacePower, Or, Xor, And, Lshift, Rshift, Add,
        Subtract, Multiply, MatMultiply, FloorDivide, TrueDivide, Modulo,
        Power,
        //CG>
        Contains,
    };

    struct Value {
        enum Tag { Nil, Int, Str, Obj };

        constexpr Value () : _v (0) {}
        constexpr Value (int arg) : _v (arg * 2 + 1) {}
        constexpr Value (Q const& arg) : _v (arg._id * 4 + 2) {}
                  Value (char const* arg);
        constexpr Value (Object const* arg) : _p (arg) {}
        constexpr Value (Object const& arg) : _p (&arg) {}
                  Value (E, char const* ="", Value ={}); // also raises

        operator int () const { return (intptr_t) _v >> 1; }
        operator char const* () const;
        auto operator-> () const { return _o; }

        template< typename T > // return null pointer if not of required type
        auto ifType () const { return check(T::info) ? (T*) _o : nullptr; }

        template< typename T > // type-asserted safe cast via Object::type()
        auto asType () const -> T& { verify(T::info); return *(T*) _o; }

        auto tag () const {
            return _v == 0 ? Nil : // all bits 0
                   _v & 1  ? Int : // bit 0 set
                   _v & 2  ? Str : // bit 1 set, ptr shifted 2 up
                             Obj;  // bits 0 and 1 clear, ptr stored as is
        }

        auto id () const { return _v; }
        auto obj () const -> Object& { return *_o; }

        auto isOk  () const { return _v != 0; }
        auto isNil () const { return _v == 0; }
        auto isInt () const { return (_v&1) == Int; }
        auto isStr () const { return (_v&3) == Str; }
        auto isObj () const { return (_v&3) == 0 && _v != 0; }
        auto isQid () const { return isStr() && (_v >> 18) == 0; }

        inline auto isNone  () const -> bool;
        inline auto isFalse () const -> bool;
        inline auto isTrue  () const -> bool;
               auto isBool  () const { return isFalse() || isTrue(); }

        auto asObj () const -> Object&; // create int/str object if needed
        auto asInt () const -> int64_t;
        auto asQid () const -> uint16_t { return isQid() ? _v >> 2 : 0; }

        static auto asBool (bool f) -> Value;
        auto truthy () const -> bool;
        auto invert () const { return asBool(!truthy()); }

        auto operator== (Value) const -> bool;
        auto unOp (UnOp op) const -> Value;
        auto binOp (BinOp op, Value rhs) const -> Value;

        auto take () { Value r = *this; *this = {}; return r; }

        inline void marker () const;
        void dump (char const* msg =nullptr) const;

        // see RawIter in data.cpp
        inline auto begin () const -> RawIter;
        inline auto end () const -> RawIter;
    private:
        auto check (Type const& t) const -> bool;
        void verify (Type const& t) const;

        union {
            uintptr_t _v;
            void const* _p;
            Object* _o;
        };
    };

    extern Value const Null;
    extern Value const False;
    extern Value const True;
    extern Value const Empty; // Tuple

    template< typename T >
    struct VecOf : private Vec {
        uint32_t _fill = 0;

        constexpr VecOf () =default;
        template< size_t N > // auto-determine the array's item count
        constexpr VecOf (T const (&ary)[N])
                    : Vec (&ary, N * sizeof (T)), _fill (N) {}
        constexpr VecOf (T const* ptr, uint32_t num)
                    : Vec (ptr, num * sizeof (T)), _fill (num) {}

        auto cap () const { return Vec::cap() / sizeof (T); }
        auto adj (uint32_t num) { return Vec::adj(num * sizeof (T)); }

        constexpr auto size () const { return _fill; }
        constexpr auto begin () const { return (T*) Vec::ptr(); }
        constexpr auto end () const { return begin() + _fill; }
        auto operator[] (uint32_t i) const -> T& { return begin()[i]; }

        auto relPos (int i) const { return i < 0 ? i + _fill : i; }

        void move (uint32_t pos, uint32_t num, int off) {
            memmove((void*) (begin() + pos + off),
                        (void const*) (begin() + pos), num * sizeof (T));
        }
        void wipe (uint32_t pos, uint32_t num) {
            memset((void*) (begin() + pos), 0, num * sizeof (T));
        }

        void insert (uint32_t idx, uint32_t num =1) {
            if (_fill > cap())
                _fill = cap();
            if (idx > _fill) {
                num += idx - _fill;
                idx = _fill;
            }
            auto need = _fill + num;
            if (need > cap())
                adj(need);
            move(idx, _fill - idx, num);
            wipe(idx, num);
            _fill += num;
        }

        void remove (uint32_t idx, uint32_t num =1) {
            if (_fill > cap())
                _fill = cap();
            if (idx >= _fill)
                return;
            if (num > _fill - idx)
                num = _fill - idx;
            move(idx + num, _fill - (idx + num), -num);
            _fill -= num;
        }

        auto find (T v) const -> uint32_t {
            for (auto& e : *this)
                if (e == v)
                    return &e - begin();
            return _fill;
        }

        void push (T v, uint32_t idx =0) {
            insert(idx);
            begin()[idx] = v;
        }

        void append (T v) { push(v, _fill); } // push to end

        auto pull (uint32_t idx =0) -> T {
            if (idx >= _fill)
                return {};
            T v = begin()[idx];
            remove(idx);
            return v;
        }

        auto pop () { return pull(_fill-1); } // pull from end

        void clear () { _fill = 0; adj(0); }

        using Vec::compact; // allow public access
    };

    using Vector = VecOf<Value>;

    void markVec (Vector const&);

    struct ArgVec {
        static constexpr auto SPREAD = 1<<16; // if "*arg" or "**kw" present

        ArgVec (Vector const& vec ={})
            : ArgVec (vec, vec.size()) {}
        ArgVec (Vector const& vec, int num, Value const* ptr)
            : ArgVec (vec, num, ptr - vec.begin()) {}
        constexpr ArgVec (Vector const& vec, int num, int off =0)
            : _vec (vec), _off (off), _num (num) {}

        auto size () const { return (uint8_t) _num; }
        auto begin () const { return _vec.begin() + _off; }
        auto end () const { return begin() + size(); }
        auto operator[] (uint32_t i) const -> Value& { return _vec[_off+i]; }

        auto parse (char const*, ...) const -> Value; // defined in type.cpp

        auto kwNum () const { return (uint8_t) (_num >> 8); }
        auto kwKey (int i) const { return begin()[size()+2*i]; }
        auto kwVal (int i) const { return begin()[size()+2*i+1]; }

        auto expSeq () const {
            return _num & SPREAD ? begin()[size()+2*kwNum()] : Value {};
        }
        auto expMap () const {
            return _num & SPREAD ? begin()[size()+2*kwNum()+1] : Value {};
        }

        Vector const& _vec;
        int _off;
    private:
        int _num;
    };

    // can't use "CG type <object>", this is the start of the type hierarchy
    struct Object : Obj {
        static const Type info;
        virtual auto type () const -> Type const& { return info; }
        virtual void repr  (Buffer&) const;

        virtual auto call  (ArgVec const&) const -> Value;
        virtual auto unop  (UnOp) const -> Value;
        virtual auto binop (BinOp, Value) const -> Value;
        virtual auto attr  (Value, Value&) const -> Value;
        virtual auto len   () const -> uint32_t;
        virtual auto getAt (Value) const -> Value;
        virtual auto setAt (Value, Value) -> Value;
        virtual auto iter  () const -> Value;
        virtual auto next  () -> Value;
        virtual auto copy  (Range const&) const -> Value;
        virtual auto store (Range const&, Object const&) -> Value;

        auto sliceGetter (Value k) const -> Value;
        auto sliceSetter (Value k, Value v) -> Value;
    };

    struct StaticObj : Object {
        // these are not for use on the heap and there's no cleanup to be done
        auto operator new (size_t) -> void* =delete;
        void operator delete (void*) {}
    };

    void Value::marker () const { if (isObj()) mark(obj()); }

    //CG1 type <none>
    struct None : StaticObj {
        void repr (Buffer&) const override;

        auto binop (BinOp, Value) const -> Value override;

        static None const noneObj;
    private:
        constexpr None () =default; // can't construct more instances
    };

    //CG1 type bool
    struct Bool : StaticObj {
        auto unop (UnOp) const -> Value override;

        static Bool const trueObj, falseObj;
    private:
        constexpr Bool () =default; // can't construct more instances
    };

    auto Value::isNone  () const -> bool { return _o == &None::noneObj; }
    auto Value::isFalse () const -> bool { return _o == &Bool::falseObj; }
    auto Value::isTrue  () const -> bool { return _o == &Bool::trueObj; }

    //CG1 type int
    struct Int : Object {
        static auto make (int64_t i) -> Value;
        static auto conv (char const* s) -> Value;

        constexpr Int (int64_t i64) : _i64 (i64) {}

        operator int64_t () const { return _i64; }

        auto unop (UnOp) const -> Value override;
        auto binop (BinOp, Value) const -> Value override;

    private:
        int64_t _i64 __attribute__((packed));
    }; // packing gives a better fit on 32b arch, and has no effect on 64b

    using ByteVec = VecOf<uint8_t>;

    struct RawIter {
        RawIter (Value obj, Value pos ={})
            : _obj (obj), _pos (pos.isNil() && obj.isObj() ? obj->iter() : pos) {}

        auto stepper () -> Value;

        // range-based for loops for vectors, iterators, and generators, see
        // https://www.nextptr.com/tutorial/ta1208652092/how-cplusplus-rangebased-for-loop-works
        auto operator* () { return _val; }
        auto operator!= (RawIter const&) -> bool;
        void operator++ () { _val = {}; }
    protected:
        Value _obj, _pos, _val;
    };

    //CG1 type <iterator>
    struct Iterator : Object, RawIter {
        Iterator (Value obj, Value pos ={}) : RawIter (obj, pos) {}

        auto next () -> Value override { return stepper(); }

        void marker () const override { _obj.marker(); }
    };

    auto Value::begin () const -> RawIter { return *this; }
    auto Value::end () const -> RawIter { return {{}, 0}; }

    //CG1 type range
    struct Range : Object {
        auto len () const -> uint32_t override;
        auto getAt (Value k) const -> Value override { return _from + k * _by; }
        auto iter () const -> Value override { return 0; }

        Range (int from, int to, int by) : _from (from), _to (to), _by (by) {}

        int32_t _from, _to, _by;
    };

    //CG1 type slice
    struct Slice : Object {
        auto asRange (int sz) const -> Range;

    private:
        Slice (Value off, Value num, Value step)
                : _off (off), _num (num), _step (step) {}

        Value _off, _num, _step;
    };

    //CG1 type bytes
    struct Bytes : Object, ByteVec {
        constexpr Bytes () =default;
        Bytes (void const*, uint32_t =0);

        auto unop (UnOp) const -> Value override;
        auto binop (BinOp, Value) const -> Value override;
        auto len () const -> uint32_t override { return _fill; }
        auto getAt (Value k) const -> Value override;
        auto iter () const -> Value override { return 0; }
        auto copy (Range const&) const -> Value override;
    };

    //CG1 type str
    struct Str : Bytes {
        Str (char const* s, int n =-1);

        operator char const* () const { return (char const*) begin(); }

        auto unop (UnOp) const -> Value override;
        auto binop (BinOp, Value) const -> Value override;
        auto getAt (Value k) const -> Value override;
    };

// see type.cpp - collection types and type system

    //CG1 type <buffer>
    struct Buffer : Bytes {
        void repr (Buffer& buf) const override { Object::repr(buf); }

        ~Buffer () override;

        void write (uint8_t const* ptr, uint32_t num);
        void putc (char v) { write((uint8_t const*) &v, 1); }
        void puts (char const* s) { write((uint8_t const*) s, strlen(s)); }
        void print (char const* fmt, ...);

        auto operator<< (Value v) -> Buffer&;
        auto operator<< (char c) -> Buffer& { putc(c); return *this; }
        auto operator<< (int i) -> Buffer& { putInt(i); return *this; }
        auto operator<< (char const* s) -> Buffer& { puts(s); return *this; }

    private:
        int splitInt (uint32_t val, int base, uint8_t* buf);
        void putFiller (int n, char fill);
        void putInt (int val, int =10, int =0, char =' ');
    };

    struct VaryVec : private ByteVec {
        constexpr VaryVec (void const* ptr =nullptr, uint32_t num =0)
                    : ByteVec ((uint8_t const*) ptr, num) {}

        using ByteVec::size;
        using ByteVec::clear;

        auto first () const { return begin(); }
        auto limit () const { return begin() + pos(_fill); }

        auto atGet (uint32_t i) const { return begin() + pos(i); }
        auto atLen (uint32_t i) const -> uint32_t { return pos(i+1) - pos(i); }
        void atAdj (uint32_t idx, uint32_t num);
        void atSet (uint32_t i, void const* ptr, uint32_t num);

        void insert (uint32_t idx, uint32_t num =1);
        void remove (uint32_t idx, uint32_t num =1);
    private:
        auto pos (uint32_t i) const -> uint16_t& {
            return ((uint16_t*) begin())[i];
        }
    };

    //CG1 type <lookup>
    struct Lookup : StaticObj {
        struct Item { Q k; Value v; };

        constexpr Lookup (Lookup const* chain =nullptr) : _chain (chain) {}
        template< size_t N > // auto-determine the array's item count
        constexpr Lookup (Item const (&items)[N], Lookup const* chain =nullptr)
            : _items (items), _count (N), _chain (chain) {}
            
        auto operator[] (Q) const -> Value;

        auto len () const -> uint32_t override { return _count; }
        auto getAt (Value k) const -> Value override;

        auto attrDir (Value) const -> Lookup const*; // can access private info
    private:
        Item const* _items {nullptr};
        uint32_t _count {0};
        Lookup const* _chain;

        friend struct Exception; // to get exception's string name
    };

    //CG1 type tuple
    struct Tuple : Object, Vector {
        constexpr Tuple () =default;
        Tuple (Value);
        Tuple (ArgVec const&);

        void printer (Buffer&, char const*) const;

        auto len () const -> uint32_t override { return _fill; }
        auto getAt (Value k) const -> Value override;
        auto iter () const -> Value override { return 0; }
        auto copy (Range const&) const -> Value override;

        void marker () const override { markVec(*this); }

        static Tuple const emptyObj;
    };

    //CG1 type list
    struct List : Tuple {
        using Tuple::Tuple;

        //CG: wrap List pop append clear
        auto pop (int idx) -> Value;
        auto append (Value v) -> Value;
        auto clear () -> Value { Vector::clear(); return {}; }

        auto setAt (Value k, Value v) -> Value override;
        auto store (Range const&, Object const&) -> Value override;
    };

    //CG1 type set
    struct Set : List {
        using List::List;

        auto find (Value v) const -> uint32_t;

        struct Proxy { Set& s; Value v;
            operator bool () const { return ((Set const&) s).has(v); }
            auto operator= (bool) -> bool;
        };

        // operator[] is problematic when the value is an int
        auto has (Value key) const -> bool { return find(key) < size(); }
        auto has (Value key) -> Proxy { return {*this, key}; }

        auto binop (BinOp, Value) const -> Value override;

        auto getAt (Value k) const -> Value override;
        auto setAt (Value k, Value v) -> Value override;
    };

    //CG1 type dict
    struct Dict : Set {
        constexpr Dict (Object const* ch =nullptr) : _chain (ch) {}
        Dict (Value);

        struct Proxy { Dict& d; Value k;
            operator Value () const { return ((Dict const&) d).at(k); }
            auto operator= (Value v) -> Value;
        };

        auto at (Value key) const -> Value;
        auto at (Value key) -> Proxy { return {*this, key}; }

        auto getAt (Value k) const -> Value override { return at(k); }
        auto setAt (Value k, Value v) -> Value override { return at(k) = v; }

        //CG: wrap Dict keys values items
        auto keys () -> Value;
        auto values () -> Value;
        auto items () -> Value;

        void marker () const override;

        Object const* _chain {nullptr};
    };

    //CG1 type type
    struct Type : Dict {
        using Factory = auto (*)(ArgVec const&,Type const*) -> Value;

        constexpr Type (Value s, Lookup const* a =nullptr, Factory f =noFactory)
                        : Dict (a), _name (s), _factory (f) {}

        auto call (ArgVec const& args) const -> Value override  {
            return _factory(args, this);
        }
        auto attr (Value name, Value&) const -> Value override {
            return getAt(name);
        }

        Value _name;
    private:
        Factory _factory;

        static auto noFactory (ArgVec const&, Type const*) -> Value;
    };

    //CG1 type class
    struct Class : Type {
    private:
        Class (ArgVec const& args);
    };

    //CG1 type super
    struct Super : Object {
        void marker () const override { _sclass.marker(); _sinst.marker(); }
    private:
        Super (Value sclass, Value sinst) : _sclass (sclass), _sinst (sinst) {}

        Value _sclass;
        Value _sinst;
    };

    // can't use CG, because type() can't be auto-generated
    struct Inst : Dict {
        static auto create (ArgVec const&, Type const*) -> Value;
        static Lookup const attrs;
        static Type info;
        void repr (Buffer&) const override;

        auto type () const -> Type const& override { return *(Type*) _chain; }
        auto attr (Value name, Value& self) const -> Value override {
            self = this;
            return getAt(name);
        }

    private:
        Inst (ArgVec const& args, Class const& cls);
    };

// see stack.cpp - events, stacklets, and various call mechanisms

    //CG1 type event
    struct Event : Object {
        ~Event () override { deregHandler(); set(); }

        auto unop (UnOp) const -> Value override;
        auto binop (BinOp, Value) const -> Value override;

        void marker () const override { markVec(_queue); }

        auto regHandler () -> uint32_t;
        void deregHandler ();

        operator bool () const { return _value; }

        //CG: wrap Event wait set clear
        auto set () -> Value ;
        auto clear () -> Value { _value = false; return {}; }
        auto wait () -> Value ;

        static int queued;
        static Vector triggers;
    private:
        Vector _queue;
        bool _value = false;
        int8_t _id = -1;
    };

    //CG1 type <stacklet>
    struct Stacklet : List {
        void repr (Buffer& buf) const override { Object::repr(buf); }

        auto iter () const -> Value override { return this; }

        void resumeCaller (Value v ={});

        void marker () const override;

        Stacklet* _caller = nullptr;
        Value _transfer;

        static void yield (bool =false);
        static auto suspend (Vector& =Event::triggers) -> Value;
        static auto runLoop () -> bool;

        virtual auto run () -> bool =0;
        virtual void raise (Value);

        static void exception (Value); // a safe way to current->raise()
        static void gcAll ();

        // see https://en.cppreference.com/w/c/atomic and
        // https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
        static void setPending (uint32_t n) {
            __atomic_fetch_or(&pending, 1<<n, __ATOMIC_RELAXED);
        }
        static auto clearAllPending () -> uint32_t {
            return __atomic_fetch_and(&pending, 0, __ATOMIC_RELAXED);
        }

        static List ready;
        static Stacklet* current;
        static volatile uint32_t pending;
    };

    //CG1 type <module>
    struct Module : Dict {
        void repr (Buffer&) const override;

        constexpr Module (Value nm, Object const& lu =builtins)
            : Dict (&lu), _name (nm) {}

        auto attr (Value name, Value&) const -> Value override {
            Value v = getAt(name);
            return v.isNil() && name == Q(0,"__name__") ? _name : v;
        }

        static Dict builtins;
        static Dict loaded;

        Value _name;
    };

    //CG1 type <function>
    struct Function : StaticObj {
        using Prim = auto (*)(ArgVec const&) -> Value;

        constexpr Function (Prim f) : _func (f) {}

        auto call (ArgVec const& args) const -> Value override {
            return _func(args);
        }

    private:
        Prim _func;
    };

    // Template trickery - this wraps several different argument calls into a
    // virtual Method::Base object, which Method can then call in a generic way.

    //CG1 type <method>
    struct Method : StaticObj {
    private:
        // Method objects point to these base instances to make virtual calls
        struct Base {
            virtual auto objCall (Object&, ArgVec const&) const -> Value = 0;
        };

        template< typename M >
        struct Wrap : Base {
            M _m;
            constexpr Wrap (M m) : _m (m) {}
            auto objCall (Object& o, ArgVec const& a) const -> Value override {
                return argConv(_m, o, a);
            }
        };

        // T::meth() -> V
        template< typename T, typename V >
        static auto argConv (V (T::*m)(), Object& o, ArgVec const&) -> Value {
            return (((T&) o).*m)();
        }
        // T::meth(U) -> V
        template< typename T, typename U, typename V >
        static auto argConv (V (T::*m)(U), Object& o, ArgVec const& a) -> Value {
            return (((T&) o).*m)(a[1]);
        }
        // T::meth(ArgVec const&) -> Value
        template< typename T >
        static auto argConv (Value (T::*m)(ArgVec const&), Object& o, ArgVec const& a) -> Value {
            return (((T&) o).*m)(a);
        }

        Base const& _meth;
    public:
        constexpr Method (Base const& meth) : _meth (meth) {}

        auto call (ArgVec const& args) const -> Value override {
            return _meth.objCall(args[0].obj(), args);
        }

        template< typename M >
        constexpr static auto wrap (M meth) -> Wrap<M> { return meth; }
    };

// see builtin.cpp - exceptions and auto-generated built-in tables

    //CG1 type <exception>
    struct Exception : Tuple {
        void repr (Buffer&) const override;

        auto binop (BinOp, Value) const -> Value override;

        void marker () const override;

        //CG: wrap Exception trace
        auto trace () -> Value;
        void addTrace (uint32_t off, Value bc);

        static auto create (E, ArgVec const&) -> Value; // diff API
        static Lookup const bases; // this maps the derivation hierarchy
        static auto findId (Function const&) -> int; // find in builtinsMap
        static Lookup const attrs;
    private:
        Exception (E exc, ArgVec const& args);
        ~Exception () override { adj(0); } // needs explicit cleanup

        E _code;
    };

// defined outside of the Monty core itself, e.g. in main.cpp cq pyvm.cpp
    auto vmImport (char const* name) -> uint8_t const*;
    auto vmLaunch (void const* data) -> Stacklet*;
}
