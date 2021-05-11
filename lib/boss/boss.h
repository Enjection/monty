#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <setjmp.h>

// see https://interrupt.memfault.com/blog/asserts-in-embedded-systems
#ifdef NDEBUG
#define ensure(exp) ((void)0)
#elif NATIVE
#include <cassert>
#define ensure(exp) assert(exp)
#else // ARM-specific code
#define ensure(exp)                                  \
    if (exp) ; else {                                \
      void* pc;                                      \
      asm volatile ("mov %0, pc" : "=r" (pc));       \
      boss::failAt(pc, __builtin_return_address(0)); \
    }
#endif

namespace boss {
    auto veprintf(void(*)(void*,int), void*, char const* fmt, va_list ap) -> int;
    void debugf (const char* fmt, ...);
    [[noreturn]] void failAt (void const*, void const*);

    struct Pool {
        constexpr static auto BUFLEN = 192;

        Pool (void* ptr, size_t len);

        auto idOf (void const* p) const -> uint8_t {
            ensure(bufs + 1 <= p && p < bufs + nBuf);
            return ((uint8_t const*) p - bufs[0].b) / BUFLEN;
        }

        auto tag (uint8_t i) -> uint8_t& { return bufs[0].b[i]; }
        auto tag (uint8_t i) const -> uint8_t { return bufs[0].b[i]; }
        auto tagOf (void const* p) -> uint8_t& { return tag(idOf(p)); }

        auto hasFree () { return tag(0) != 0; }

        auto allocate () -> uint8_t*;
        void release (uint8_t i) { tag(i) = tag(0); tag(0) = i; }
        void releasePtr (uint8_t* p) { release(idOf(p)); }

        auto items (uint8_t i =0) const -> int;
        void check () const;

        auto operator[] (uint8_t i) -> uint8_t* { return bufs[i].b; }
    private:
        uint8_t nBuf;
        // make sure there's always room for jmp_buf, even if it exceeds BUFLEN
        union Buffer { jmp_buf j; uint8_t b [BUFLEN]; } *bufs;

        static_assert(BUFLEN < 256, "buffer fill must fit in a uint8_t");
    };

    extern Pool pool;

    struct Fiber {
        using Fid_t = uint8_t;

        struct Queue {
            auto isEmpty () const { return first == 0; }
            auto length () const { return isEmpty() ? 0 : pool.items(first); }

            auto pull () -> Fid_t;
            void insert (Fid_t i);
            void append (Fid_t i);

            auto expire (uint16_t now, uint16_t& limit) -> int;
        private:
            Fid_t first =0, last =0;
        };

        auto id () const { return pool.idOf(this); }

        static auto at (Fid_t i) -> Fiber& { return *(Fiber*) pool[i]; }
        static auto runLoop () -> bool;
        static void app ();
        static void processPending ();
        static void msWait (uint16_t ms) { suspend(timers, ms); }
        static auto suspend (Queue&, uint16_t ms =60'000) -> int;
        static void resume (Fid_t f, int i) { at(f).stat = i; ready.append(f); }
        static void duff (uint32_t* dst, uint32_t const* src, uint32_t cnt);

        static Fid_t curr;
        static Queue ready;
        static Queue timers;

        uint16_t timeout;
        int8_t stat;
        jmp_buf context;
        uint32_t data [];
    };

    struct Semaphore : private Fiber::Queue {
        Semaphore (int n) : count (n) {}

        void post () {
            if (++count <= 0)
                Fiber::resume(pull(), 1);
        }

        auto pend (uint32_t ms =60'000) -> int {
            return --count >= 0 ? 1 : Fiber::suspend(*this, ms);
        }

        void expire (uint16_t now, uint16_t& limit);
    private:
        int16_t count;
    };
}
