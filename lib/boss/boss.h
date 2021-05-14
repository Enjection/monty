#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <setjmp.h>

#if NATIVE
#include <cassert>
#endif

// see https://interrupt.memfault.com/blog/asserts-in-embedded-systems
#ifndef assert
#ifdef NDEBUG
#define assert(exp) ((void)0)
#else // ARM-specific code
#define assert(exp)                                  \
    if (exp) ; else {                                \
      void* pc;                                      \
      asm volatile ("mov %0, pc" : "=r" (pc));       \
      boss::failAt(pc, __builtin_return_address(0)); \
    }
#endif
#endif

namespace boss {
    auto veprintf(void(*)(void*,int), void*, char const* fmt, va_list ap) -> int;
    void debugf (const char* fmt, ...);
    [[noreturn]] void failAt (void const*, void const*);

    struct Pool {
        constexpr static auto BUFLEN = 192;

        Pool (void* ptr, size_t len);

        auto idOf (void const* p) const -> uint8_t {
            assert(bufs + 1 <= p && p < bufs + nBuf);
            return ((uint8_t const*) p - bufs[0].b) / sizeof (Buffer);
        }

        auto tag (uint8_t i) -> uint8_t& { return bufs[0].b[i]; }
        auto tag (uint8_t i) const -> uint8_t { return bufs[0].b[i]; }

        auto hasFree () { return tag(0) != 0; }

        void init ();
        auto allocate () -> uint8_t*;
        void release (uint8_t i) { tag(i) = tag(0); tag(0) = i; }
        void releasePtr (uint8_t* p) { release(idOf(p)); }

        auto items (uint8_t i) const -> int;
        void check () const;

        auto operator[] (uint8_t i) -> uint8_t* { return bufs[i].b; }
    private:
        uint8_t const nBuf;
        // make sure there's always room for jmp_buf, even if it exceeds BUFLEN
        alignas(8) union Buffer {
            uint8_t f [sizeof (jmp_buf) + 120]; // TODO slack until segmented
            uint8_t b [BUFLEN];
        } *bufs;

        static_assert(BUFLEN < 256, "buffer fill must fit in a uint8_t");
        static_assert(sizeof (Buffer) % 8 == 0);
    };

    extern Pool pool;

    struct Fiber {
        using Fid_t = uint8_t;

        struct Queue {
            auto isEmpty () const { return first == 0; }
            auto pull () -> Fid_t;
            void insert (Fid_t i);
            void append (Fid_t i);

            auto expire (uint16_t now, uint16_t& limit) -> int;
        private:
            Fid_t first =0, last =0;
        };

        static auto at (Fid_t i) -> Fiber& { return *(Fiber*) pool[i]; }
        static auto runLoop () -> bool;
        static auto create (void (*)(void*), void* =nullptr) -> Fid_t;
        static void processPending ();
        static void msWait (uint16_t ms) { suspend(timers, ms); }
        static auto suspend (Queue&, uint16_t ms =60'000) -> int;
        static void resume (Fid_t f, int i) { at(f).stat = i; ready.append(f); }
        static void duff (uint32_t* dst, uint32_t const* src, uint32_t cnt);

        static Fid_t curr;
        static Queue ready;
        static Queue timers;

        int8_t stat;
        uint16_t timeout;
        union {
            jmp_buf context;
            struct { void (*fun)(void*); void* arg; };
        };
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
