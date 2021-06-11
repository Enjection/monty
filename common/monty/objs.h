namespace monty {
    void objInit (void* ptr, size_t len);

    struct Obj {
        Obj () =default;
        virtual ~Obj () =default;

        virtual void marker () const {} // called to mark all ref'd objects

        static auto inPool (void const* p) -> bool;
        auto isCollectable () const { return inPool(this); }

        auto operator new (size_t bytes) -> void*;
        auto operator new (size_t bytes, uint32_t extra) -> void* {
            return operator new (bytes + extra);
        }
        void operator delete (void*);

        static void sweep ();   // reclaim all unmarked objects
        static void dumpAll (); // like sweep, but only to print all obj+free

        // "Rule of 5"
        Obj (Obj&&) =delete;
        Obj (Obj const&) =delete;
        auto operator= (Obj&&) -> Obj& =delete;
        auto operator= (Obj const&) -> Obj& =delete;
    };

    void mark (Obj const&);
    inline void mark (Obj const* p) { if (p != nullptr) mark(*p); }
}
