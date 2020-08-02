// see mem.cpp - objects and vectors with garbage collection
namespace Monty {

    struct Obj {
        virtual ~Obj () {}

        auto isCollectable () const -> bool;

        auto operator new (size_t bytes) -> void*;
        auto operator new (size_t bytes, size_t extra) -> void* {
            return operator new (bytes + extra);
        }
        void operator delete (void*);
    protected:
        Obj () {} // only derived objects can be instantiated

        virtual void marker () const {} // called to mark all ref'd objects
        friend void mark (Obj const&);
    };

    struct Vec {
        Vec () : extra (0), caps (0), data (nullptr) {}
        ~Vec () { resize(0); }

        auto ptr () const -> uint8_t* { return data; }
        auto cap () const -> size_t;
        auto resize (size_t bytes) -> bool;

    protected:
        uint32_t extra :8;  // for use in derived classes TODO remove
    private:
        uint32_t caps :24;  // capacity in slots, see cap() TODO in bytes
        uint8_t* data;      // points into memory pool when cap() > 0

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

} // namespace Monty

// TODO vec.cpp ... also move Vector stuff here (from array.cpp)
namespace Monty {

    template< typename T >
    struct VecOf : Vec {
        VecOf (size_t n =0) : len (n) {}

        auto operator[] (size_t idx) const -> T& {
            //assert(idx < len);
            return ((T*) ptr())[idx];
        }

        size_t len;
    };

    template< typename T >
    struct Chunk {
        Chunk (VecOf<T> const& v, size_t o =0) : vec (v), off (o) {}

        auto operator[] (size_t idx) const -> T& {
            return ((T*) vec.ptr())[off+idx];
        }

        VecOf<T> const& vec;
        size_t off;
    };

} // namespace Monty

// see type.cpp - basic object types and type system
namespace Monty {

    // forward decl's
    enum class UnOp : uint8_t;
    enum class BinOp : uint8_t;
    struct Object;
    struct Type;
    struct Lookup;

    // TODO keep these aliases until codegen.py has been updated
    using Value = struct Val;
    using LookupObj = Lookup;
    using TypeObj = Type;

    struct Val {
        enum Tag { Nil, Int, Str, Obj };

        Val () {}
        Val (int arg);
        Val (char const* arg);
        Val (Object const* arg) : v ((uintptr_t) arg) {} // TODO keep?
        Val (Object const& arg) : v ((uintptr_t) &arg) {}

        operator int () const { return (intptr_t) v >> 1; }
        operator char const* () const { return (char const*) (v >> 2); }
        auto obj () const -> Object& { return *(Object*) v; }
        // TODO inline auto objPtr () const -> ForceObj;

        template< typename T > // return null pointer if not of required type
        auto ifType () const -> T* { return check(T::info) ? (T*) &obj() : 0; }

        template< typename T > // type-asserted safe cast via Object::type()
        auto asType () const -> T& { verify(T::info); return *(T*) &obj(); }

        auto tag () const -> enum Tag {
            return (v&1) != 0 ? Int : // bit 0 set
                       v == 0 ? Nil : // all bits 0
                   (v&2) != 0 ? Str : // bit 1 set, ptr shifted 2 up
                                Obj;  // bits 0 and 1 clear, ptr stored as is
        }

        auto id () const -> uintptr_t { return v; }

        auto isNil () const -> bool { return v == 0; }
        auto isInt () const -> bool { return (v&1) != 0; }
        auto isStr () const -> bool { return (v&3) == 2; }
        auto isObj () const -> bool { return (v&3) == 0 && v != 0; }

        inline auto isNone  () const -> bool;
        inline auto isFalse () const -> bool;
        inline auto isTrue  () const -> bool;
               auto isBool  () const -> bool { return isFalse() || isTrue(); }

        auto truthy () const -> bool;

        auto isEq (Val) const -> bool;
        auto unOp (UnOp op) const -> Val;
        auto binOp (BinOp op, Val rhs) const -> Val;
        void dump (char const* msg =nullptr) const; // see builtin.h

        static auto asBool (bool f) -> Val { return f ? True : False; }
        auto invert () const -> Val { return asBool(!truthy()); }

        static Val const None;
        static Val const False;
        static Val const True;
    private:
        auto check (Type const& t) const -> bool;
        void verify (Type const& t) const;

        uintptr_t v{0};
    };

    // can't use "CG3 type <object>", as type() is virtual iso override
    struct Object : Obj {
        static const Type info;
        virtual auto type () const -> Type const&;

        // virtual auto repr (BufferObj&) const -> Val; // see builtin.h
        virtual auto unop  (UnOp) const -> Val;
        virtual auto binop (BinOp, Val) const -> Val;
    };

    //CG3 type <none>
    struct None : Object {
        static const TypeObj info;
        const TypeObj& type () const override;

        //auto repr (BufferObj&) const -> Val override; // see builtin.h
        auto unop (UnOp) const -> Val override;

        static None const noneObj;
    private:
        None () {} // can't construct more instances
    };

    //CG< type bool
    struct Bool : Object {
        static Value create (const TypeObj&, int argc, Value argv[]);
        static const LookupObj attrs;
        static const TypeObj info;
        const TypeObj& type () const override;
    //CG>

        //auto repr (BufferObj&) const -> Val override; // see builtin.h
        auto unop (UnOp) const -> Val override;

        static Bool const trueObj;
        static Bool const falseObj;
    private:
        Bool () {} // can't construct more instances
    };

    //CG< type int
    struct Long : Object {
        static Value create (const TypeObj&, int argc, Value argv[]);
        static const LookupObj attrs;
        static const TypeObj info;
        const TypeObj& type () const override;
    //CG>

        Long (int64_t v) : i (v) {}

        operator int64_t () const { return i; }

        //auto repr (BufferObj&) const -> Val override; // see builtin.h
        auto unop (UnOp) const -> Val override;

    private:
        int64_t i;
    };

    //CG< type type
    struct Type : Object {
        static Value create (const TypeObj&, int argc, Value argv[]);
        static const LookupObj attrs;
        static const TypeObj info;
        const TypeObj& type () const override;
    //CG>

        typedef auto (*Factory)(Type const&,int,Val[]) -> Val;

        char const* name;
        Factory const factory;

        Type (char const* s, Factory f =noFactory, Lookup const* a =nullptr)
            : name (s), factory (f) { /* TODO chain = a; */ }

        //auto call (Chunk<Val>& args) const -> Val override;
        //auto attr (char const*, Val&) const -> Val override;

    private:
        static auto noFactory (Type const&,int,Val[]) -> Val;
    };

    void mark (VecOf<Val> const& v);

    auto Val::isNone  () const -> bool { return &obj() == &None::noneObj; }
    auto Val::isFalse () const -> bool { return &obj() == &Bool::falseObj; }
    auto Val::isTrue  () const -> bool { return &obj() == &Bool::trueObj; }

} // namespace Monty

// see array.cpp - vectors, arrays, and other derived types
namespace Monty {
    struct Vector : private Vec {
        Vector (size_t bits);

        auto length () const -> size_t { return fill; }
        int width () const { auto b = 1<<extra; return b < 8 ? -b : b/8; }
        size_t widthOf (int num) const { return ((num << extra) + 7) >> 3; }

        auto getPtr (int idx) const -> uint8_t*;
        auto getInt (int idx) const -> int;
        auto getIntU (int idx) const -> uint32_t;

        void set (int idx, void const* ptr);
        void set (int idx, int val);

        void ins (size_t idx, int num =1);
        void del (size_t idx, int num =1);

    protected:
        uint32_t fill{0}; // in elements
    };

    //CG< type array
    struct Array : Object {
        static Value create (const TypeObj&, int argc, Value argv[]);
        static const LookupObj attrs;
        static const TypeObj info;
        const TypeObj& type () const override;
    //CG>

        Array (char atype);

        auto get (int idx) const -> Val;
        void set (int idx, Val val);

        union {
            Chunk<Val>      v;
            Chunk<int8_t>   i8;
            Chunk<int16_t>  i16;
            Chunk<int32_t>  i32;
            Chunk<uint8_t>  u8;
            Chunk<uint16_t> u16;
            Chunk<uint32_t> u32;
        };
    };

} // namespace Monty
