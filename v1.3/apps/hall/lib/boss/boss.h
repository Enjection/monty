#include "hall.h"
#include <cstdarg>
#include <cstddef>
#include <setjmp.h>

// see https://interrupt.memfault.com/blog/asserts-in-embedded-systems
#ifdef NDEBUG
#define ensure(exp) ((void)0)
#elif NATIVE
#include <cassert>
#define ensure(exp) assert(exp)
#else
#define ensure(exp)                                 \
  do                                                \
    if (!(exp)) {                                   \
      void* pc;                                     \
      asm volatile ("mov %0, pc" : "=r" (pc));      \
      void const* lr = __builtin_return_address(0); \
      boss::failAt(pc, lr);                         \
    }                                               \
  while (false)
#endif

namespace boss {
    using namespace hall;

    [[noreturn]] void failAt (void const*, void const*) __attribute__ ((weak));

    auto veprintf(void(*)(void*,int), void*, char const* fmt, va_list ap) -> int;
    void debugf (const char* fmt, ...);

#if NATIVE
    template <int N =100, int SZ =1024>
#else
    template <int N =10, int SZ =192>
#endif
    struct Pool {
        enum { NBUF=N, SZBUF=SZ };

        Pool () {
            for (int i = 0; i < N-1; ++i)
                tag(i) = i+1;
            tag(N-1) = 0;
        }

        auto idOf (void const* p) const -> uint8_t {
            ensure(buffers + 1 <= p && p < buffers + N);
            return ((uint8_t const*) p - buffers[0]) / SZ;
        }

        auto tag (uint8_t i) -> uint8_t& { return buffers[0][i]; }
        auto tagOf (void const* p) -> uint8_t& { return tag(idOf(p)); }

        auto hasFree () { return tag(0) != 0; }

        auto allocate () {
            auto n = tag(0);
            ensure(n != 0);
            tag(0) = tag(n);
            tag(n) = 0;
            return buffers[n];
        }

        void release (uint8_t i) { tag(i) = tag(0); tag(0) = i; }
        void releasePtr (uint8_t* p) { release(idOf(p)); }

        auto items (uint8_t i) {
            int n = 0;
            do {
                ++n;
                i = tag(i);
            } while (i != 0);
            return n;
        }

        auto numFree () const {
            return items(0);
        }

        void check () {
            bool inUse [N];
            for (int i = 0; i < N; ++i)
                inUse[i] = 0;
            for (int i = tag(0); i != 0; i = tag(i)) {
                ensure(!inUse[i]);
                inUse[i] = true;
            }
        }

        auto operator[] (uint8_t i) -> uint8_t* { return buffers[i]; }

    private:
        uint8_t buffers [N][SZ];

        static_assert(N <= 256, "buffer id must fit in a uint8_t");
        static_assert(N <= SZ, "free chain must fit in buffer[0]");
    };

    extern Pool<> pool;

    struct Queue {
        auto isEmpty () const { return first == 0; }
        auto length () const { return isEmpty() ? 0 : pool.items(first); }

        auto pull () -> uint8_t;
        void insert (uint8_t i);
        void append (uint8_t i);

        auto expire (uint16_t now, uint16_t& limit) -> int;
    private:
        uint8_t first =0, last =0;
    };

    struct Fiber {
        auto operator new (size_t, void* p) -> void* { return p; }

        auto id () const { return pool.idOf(this); }

        static auto at (uint8_t i) -> Fiber& { return *(Fiber*) pool[i]; }
        static void runLoop ();
        static auto suspend (Queue&, uint16_t ms =60'000) -> int;
        static void resume (uint8_t fid, int i) {
            at(fid)._status = i;
            ready.append(fid);
        }

        static uint8_t curr;
        static Queue ready;
        static void (*app)();

        uint16_t _timeout;
        int8_t _status;
        jmp_buf _context;
        uint32_t _data [];
    };

    struct Semaphore : private Queue {
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
