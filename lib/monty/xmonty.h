namespace Monty {

// see mem.cpp - objects and vectors with garbage collection

    struct Obj {
        virtual ~Obj () {}

        auto isCollectable () const -> bool;

        auto operator new (size_t bytes) -> void*;
        auto operator new (size_t bytes, size_t extra) -> void* {
            return operator new (bytes + extra);
        }
        void operator delete (void*);
    protected:
        constexpr Obj () {} // only derived objects can be instantiated

        virtual void marker () const {} // called to mark all ref'd objects
        friend void mark (Obj const&);
    };

    struct Vec {
        constexpr Vec (void const* ptr =nullptr, size_t len =0) // TODO caps b
            : size (len/sizeof (void*)/2), data ((uint8_t*) ptr) {}
        ~Vec () { (void) resize(0); }

        Vec (Vec const&) = delete;
        Vec& operator= (Vec const&) = delete;
        // TODO Vec (Vec&& v); Vec& operator= (Vec&& v);

        auto isResizable () const -> bool;

        auto ptr () const -> uint8_t* { return data; }
        auto cap () const -> size_t;
        auto resize (size_t bytes) -> bool;

    private:
        uint32_t size;  // capacity in slots, see cap() TODO in bytes
        uint8_t* data;  // points into memory pool when cap() > 0

        auto findSpace (size_t) -> void*; // hidden private type
        friend void compact ();
    };

    void setup (uintptr_t* base, size_t bytes); // configure the memory pool
    auto avail () -> size_t; // free bytes between the object & vector areas

    inline void mark (Obj const* p) { if (p != nullptr) mark(*p); }
    void mark (Obj const&);
    void sweep ();   // reclaim all unmarked objects
    void compact (); // reclaim and compact unused vector space

    extern void (*panicOutOfMemory)(); // triggers an assertion by default

// see chunk.cpp - typed and chunked access to vectors

    struct Value; // forward decl

    template< typename T >
    struct VecOf : Vec {
        auto ptr () const -> T* { return (T*) Vec::ptr(); }
        auto cap () const -> size_t { return Vec::cap() / sizeof (T); }

        auto operator[] (size_t idx) const -> T { return ptr()[idx]; }
        auto operator[] (size_t idx) -> T& { return ptr()[idx]; }

        void move (size_t pos, size_t num, int off) {
            memmove((void*) (ptr() + pos + off),
                        (void const*) (ptr() + pos), num * sizeof (T));
        }
        void wipe (size_t pos, size_t num) {
            memset((void*) (ptr() + pos), 0, num * sizeof (T));
        }
    };

    struct Chunk {
        Chunk (Vec& v) : vec (v) {}

        auto isValid () const -> bool { return (void*) &vec != nullptr; }

        auto asVec () const -> Vec& { return vec; }
        template< typename T >
        auto asVecOf () const -> VecOf<T>& { return (VecOf<T>&) vec; }

        Vec& vec;         // parent vector
    protected:
        size_t off {0};   // starting offset, in typed units
        size_t len {~0U}; // maximum length, in typed units
    };

    template< typename T >
    struct ChunkOf : Chunk {
        using Chunk::off; // make public
        using Chunk::len; // make public

        constexpr ChunkOf (Vec& v) : Chunk (v) {}

        auto length () const -> size_t {
            auto& vot = asVecOf<T>();
            auto n = vot.cap() - off;
            return n > len ? len : n >= 0 ? n : 0;
        }

        auto operator[] (size_t idx) const -> T& {
            // assert(idx < length());
            auto& vot = asVecOf<T>();
            return vot[off+idx];
        }

        void insert (size_t idx, size_t num =1) {
            auto& vot = asVecOf<T>();
            if (len > vot.cap())
                len = vot.cap();
            if (idx > len) {
                num += idx - len;
                idx = len;
            }
            auto need = (off + len + num) * sizeof (T);
            if (need > vec.cap())
                vec.resize(need);
            vot.move(off + idx, len - idx, num);
            vot.wipe(off + idx, num);
            len += num;
        }

        void remove (size_t idx, size_t num =1) {
            auto& vot = asVecOf<T>();
            if (len > vot.cap())
                len = vot.cap();
            if (idx >= len)
                return;
            if (num > len - idx)
                num = len - idx;
            vot.move(off + idx + num, len - (idx + num), -num);
            len -= num;
        }
    };

    struct Segment : Chunk {
        static auto create (char type, Vec& vec) -> Segment&;

        using Chunk::Chunk;
        virtual ~Segment () {}

        operator Value () const;
        Segment& operator= (Value);

        virtual auto typ () const -> char;
        virtual auto get (int idx) const -> Value;
        virtual void set (int idx, Value val);
        virtual void ins (size_t idx, size_t num =1);
        virtual void del (size_t idx, size_t num =1);
    };

    template< char C, typename T >
    struct SegmentOf : Segment {
        using Segment::Segment;

        auto typ () const -> char override { return C; }
        auto get (int i) const -> Value override;
        void set (int i, Value v) override;
        void ins (size_t i, size_t n =1) override { cot().insert(i, n); }
        void del (size_t i, size_t n =1) override { cot().remove(i, n); }

    private:
        auto cot () -> ChunkOf<T>& { return *(ChunkOf<T>*) this; }
    };

    void mark (Segment const&);
    void mark (ChunkOf<Segment> const&);
    void mark (ChunkOf<Value> const&);

// see type.cpp - basic object types and type system

    // forward decl's
    enum class UnOp : uint8_t;
    enum class BinOp : uint8_t;
    struct Callable;
    struct Object;
    struct Lookup;
    struct Type;

    struct Value {
        enum Tag { Nil, Int, Str, Obj };

        constexpr Value () : v (0) {}
                  Value (int arg);
                  Value (char const* arg);
        constexpr Value (Object const* arg) : p (arg) {} // TODO keep?
        constexpr Value (Object const& arg) : p (&arg) {}

        operator int () const { return (intptr_t) v >> 1; }
        operator char const* () const { return (char const*) (v >> 2); }
        auto obj () const -> Object& { return *(Object*) v; }
        // TODO inline auto objPtr () const -> ForceObj;

        template< typename T > // return null pointer if not of required type
        auto ifType () const -> T* { return check(T::info) ? (T*) &obj() : 0; }

        template< typename T > // type-asserted safe cast via Object::type()
        auto asType () const -> T& { verify(T::info); return *(T*) &obj(); }

        auto tag () const -> Tag {
            return (v&1) != 0 ? Int : // bit 0 set
                       v == 0 ? Nil : // all bits 0
                   (v&2) != 0 ? Str : // bit 1 set, ptr shifted 2 up
                                Obj;  // bits 0 and 1 clear, ptr stored as is
        }

        auto id () const -> uintptr_t { return v; }

        auto isNil () const -> bool { return v == 0; }
        auto isInt () const -> bool { return (v&1) == Int; }
        auto isStr () const -> bool { return (v&3) == Str; }
        auto isObj () const -> bool { return (v&3) == Nil && v != 0; }

        inline auto isNone  () const -> bool;
        inline auto isFalse () const -> bool;
        inline auto isTrue  () const -> bool;
               auto isBool  () const -> bool { return isFalse() || isTrue(); }

        auto truthy () const -> bool;

        auto isEq (Value) const -> bool;
        auto unOp (UnOp op) const -> Value;
        auto binOp (BinOp op, Value rhs) const -> Value;
        void dump (char const* msg =nullptr) const; // see builtin.h

        static auto asBool (bool f) -> Value { return f ? True : False; }
        auto invert () const -> Value { return asBool(!truthy()); }

        static Value const None;
        static Value const False;
        static Value const True;
    private:
        auto check (Type const& t) const -> bool;
        void verify (Type const& t) const;

        union {
            uintptr_t v;
            const void* p;
        };
    };

    inline void mark (Value v) { if (v.isObj()) mark(v.obj()); }

    // define SegmentOf<C,T>'s get & set, now that Value type is complete
    template< char C, typename T >
    auto SegmentOf<C,T>::get (int i) const -> Value {
        return (*(ChunkOf<T> const*) this)[i];
    }
    template< char C, typename T >
    void SegmentOf<C,T>::set (int i, Value v) {
        cot()[i] = v;
    }

    // can't use "CG3 type <object>", as type() is virtual iso override
    struct Object : Obj {
        static const Type info;
        virtual auto type () const -> Type const&;

        // virtual auto repr (BufferObj&) const -> Value; // see builtin.h
        virtual auto unop  (UnOp) const -> Value;
        virtual auto binop (BinOp, Value) const -> Value;
    };

    //CG3 type <none>
    struct None : Object {
        static Type const info;
        auto type () const -> Type const& override;

        //auto repr (BufferObj&) const -> Value override; // see builtin.h
        auto unop (UnOp) const -> Value override;

        static None const noneObj;
    private:
        constexpr None () {} // can't construct more instances
    };

    //CG< type bool
    struct Bool : Object {
        static auto create (Type const&,ChunkOf<Value> const&) -> Value;
        static Lookup const attrs;
        static Type const info;
        auto type () const -> Type const& override;
    //CG>

        //auto repr (BufferObj&) const -> Value override; // see builtin.h
        auto unop (UnOp) const -> Value override;

        static Bool const trueObj;
        static Bool const falseObj;
    private:
        constexpr Bool () {} // can't construct more instances
    };

    //CG< type int
    struct Fixed : Object {
        static auto create (Type const&,ChunkOf<Value> const&) -> Value;
        static Lookup const attrs;
        static Type const info;
        auto type () const -> Type const& override;
    //CG>

        constexpr Fixed (int64_t v) : i (v) {}

        operator int64_t () const { return i; }

        //auto repr (BufferObj&) const -> Value override; // see builtin.h
        auto unop (UnOp) const -> Value override;
        auto binop (BinOp, Value) const -> Value override;

    private:
        int64_t i __attribute__((packed));
    }; // packing gives a better fit on 32b arch, and has no effect on 64b

    //CG3 type <lookup>
    struct Lookup : Object {
        static Type const info;
        auto type () const -> Type const& override;

        struct Item { char const* k; Value v; };

        constexpr Lookup (Item const* p, size_t n) : items (p), count (n) {}

        //auto at (Value) const -> Value override;

        void marker () const override;

    protected:
        Item const* items;
        size_t count;
    };

    //CG< type type
    struct Type : Object {
        static auto create (Type const&,ChunkOf<Value> const&) -> Value;
        static Lookup const attrs;
        static Type const info;
        auto type () const -> Type const& override;
    //CG>

        using Factory = auto (*)(Type const&,ChunkOf<Value> const&) -> Value;

        constexpr Type (char const* s, Factory f =noFactory, Lookup const* =nullptr)
            : name (s), factory (f) { /* TODO chain = a; */ }

        //auto call (int ac, ChunkOf<Value> const& av) const -> Value override;
        //auto attr (char const*, Value&) const -> Value override;

        char const* const name;
        Factory const factory;

    private:
        static auto noFactory (Type const&,ChunkOf<Value> const&) -> Value;
    };

    auto Value::isNone  () const -> bool { return &obj() == &None::noneObj; }
    auto Value::isFalse () const -> bool { return &obj() == &Bool::falseObj; }
    auto Value::isTrue  () const -> bool { return &obj() == &Bool::trueObj; }

// see array.cpp - arrays, dicts, and other derived types

    //CG< type array
    struct Array : Object {
        static auto create (Type const&,ChunkOf<Value> const&) -> Value;
        static Lookup const attrs;
        static Type const info;
        auto type () const -> Type const& override;
    //CG>

        Array (char type ='V') : items (Segment::create(type, vec)) {}

        struct Proxy { Segment& seg; size_t idx;
            operator Value () const { return seg.get(idx); }
            Value operator= (Value v) { seg.set(idx, v); return v; }
        };

        auto operator[] (size_t idx) const -> Value { return items.get(idx); }
        auto operator[] (size_t idx) -> Proxy { return {items, idx}; }

        void marker () const override { mark(items); }

    protected:
        Vec vec;
        Segment& items;
        friend struct Segment;
    };

    //CG< type set
    struct Set : Array {
        static auto create (Type const&,ChunkOf<Value> const&) -> Value;
        static Lookup const attrs;
        static Type const info;
        auto type () const -> Type const& override;
    //CG>

    };

    //CG< type dict
    struct Dict : Set {
        static auto create (Type const&,ChunkOf<Value> const&) -> Value;
        static Lookup const attrs;
        static Type const info;
        auto type () const -> Type const& override;
    //CG>

        void marker () const override { Set::marker(); mark(vals); }

    protected:
        ChunkOf<Value> vals {vec};
    };

// see state.cpp - execution state, stacks, and callables

    //CG3 type <function>
    struct Function : Object {
        static Type const info;
        auto type () const -> Type const& override;
        using Prim = auto (*)(int,ChunkOf<Value> const&) -> Value;

        constexpr Function (Prim f) : func (f) {}

        //auto call (int ac, ChunkOf<Value> const& av) const -> Value override {
        //    return func(ac, av);
        //}

    protected:
        Prim func;
    };

    //CG3 type <boundmeth>
    struct BoundMeth : Object {
        static Type const info;
        auto type () const -> Type const& override;

        constexpr BoundMeth (Value f, Value o) : meth (f), self (o) {}

        //auto call (int ac, ChunkOf<Value> const& av) const -> Value override;

        void marker () const override { mark(meth); mark(self); }

    protected:
        Value meth;
        Value self;
    };

    //CG3 type <context>
    struct Context : Object {
        static Type const info;
        auto type () const -> Type const& override;

        enum Reg { Link, Sp, Ip, Ep, Code, Locals, Globals, Result, Extra };

        void push (Callable const&);
        void pop ();

        void marker () const override { mark(stack); }

        ChunkOf<Value> stack {vec};
    private:
        VecOf<Value> vec;
    };

// see import.cpp - importing, loading, and bytecode objects

    struct Bytecode; // hidden

    //CG3 type <callable>
    struct Callable : Object {
        static Type const info;
        auto type () const -> Type const& override;

        Callable (Bytecode const& callee) : bc (callee) {}

        auto frameSize () const -> size_t;
        auto isGenerator () const -> bool;
        auto hasVarArgs () const -> bool;

        //auto call (int ac, ChunkOf<Value> const& av) const -> Value override;

        void marker () const override;

    private:
        Bytecode const& bc;
    };

    //CG3 type <module>
    struct Module : Object {
        static Type const info;
        auto type () const -> Type const& override;

        void marker () const override { globals.marker(); }

    protected:
        Module () {}

        Dict globals;
    };

    auto loadModule (uint8_t const* addr) -> Module*;

} // namespace Monty
