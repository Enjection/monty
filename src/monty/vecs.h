namespace monty {
    struct VecSlot; // opaque

    extern VecSlot *vecLow, *vecHigh;
    extern uint8_t* vecTop;

    void vecInit (void* ptr, size_t len);

    struct Vec {
        constexpr Vec () =default;
        constexpr Vec (void const* data, uint32_t capa =0)
                    : _data ((uint8_t*) data), _capa (capa) {}
        ~Vec () { (void) adj(0); }

        static auto inPool (void const* p) -> bool {
            return vecLow < p && p < vecHigh;
        }
        static void compact (); // reclaim and compact unused vector space

        auto cap () const { return _capa; }
        auto ptr () const { return _data; }
        auto adj (size_t sz) -> bool;
    private:
        uint8_t* _data {nullptr};
        size_t _capa {0};

        auto slots () const -> uint32_t; // capacity in vecslots
        auto findSpace (uint32_t) -> void*; // hidden private type

        // "Rule of 5"
        Vec (Vec&&) =delete;
        Vec (Vec const&) =delete;
        auto operator= (Vec&&) -> Vec& =delete;
        auto operator= (Vec const&) -> Vec& =delete;
    };
}
