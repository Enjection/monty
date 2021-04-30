#include "hall.h"
#include <cstdarg>
#include <setjmp.h>

// see https://interrupt.memfault.com/blog/asserts-in-embedded-systems
#ifdef NDEBUG
#define ensure(exp) ((void)0)
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

    template <int N =30, int SZ =128>
    struct Pool {
        enum { NBUF=N, SZBUF=SZ };

        Pool () {
            for (int i = 0; i < N-1; ++i)
                tag(i) = i+1;
            irqFree = tag(N-1) = 0;
        }

        auto idOf (void const* p) const -> uint8_t {
            ensure(buffers[1] <= p && p < buffers[N]);
            return ((uint8_t const*) p - buffers[0]) / SZ;
        }

        auto tag (uint8_t i) -> uint8_t& { return buffers[0][i]; }
        auto tagOf (void const* p) -> uint8_t& { return tag(idOf(p)); }

        auto hasFree () { return tag(0) != 0 || irqFree != 0; }

        auto allocate () {
            // grab the irq free chain if the main one is empty, but do it safely
            if (tag(0) == 0) {
                BlockIRQ crit;
                irqRelease(0); // this grabs the irqFree chain
            }
            auto n = tag(0);
            ensure(n != 0);
            tag(0) = tag(n);
            tag(n) = 0;
            return buffers[n];
        }

        void release (uint8_t i) { tag(i) = tag(0); tag(0) = i; }
        void releasePtr (uint8_t* p) { release(idOf(p)); }

        // only call these versions from interrupt context, i.e. with IRQs disabled
        void irqRelease (uint8_t i) { tag(i) = irqFree; irqFree = i; }
        void irqReleasePtr (uint8_t* p) { irqRelease(idOf(p)); }

        auto items (uint8_t i) const {
            int n = 0;
            do {
                ++n;
                i = tag(i);
            } while (i != 0);
            return n;
        }

        auto numFree () const {
            int n = items(0);
            BlockIRQ crit;
            if (irqFree != 0)
                n += items(irqFree);
            return n;
        }

        void check () const {
            bool inUse [N];
            memset(inUse, 0, sizeof inUse);
            for (int i = tag(0); i != 0; i = tag(i)) {
                ensure(!inUse[i]);
                inUse[i] = true;
            }
            BlockIRQ crit;
            for (int i = irqFree; i != 0; i = tag(i)) {
                ensure(!inUse[i]);
                inUse[i] = true;
            }
        }

        auto operator[] (uint8_t i) -> uint8_t* { return buffers[i]; }

    private:
        uint8_t buffers [N][SZ];
        volatile uint8_t irqFree; // second free chain, for use from IRQ context

        static_assert(N <= 256, "buffer id must fit in a uint8_t");
        static_assert(N <= SZ, "free chain must fit in buffer[0]");
    };

    extern Pool<> pool;

    struct Queue {
        auto pull () -> uint8_t;
        void insert (uint8_t i);
        void append (uint8_t i);

        uint8_t first =0, last =0;
    };

    struct Fiber {
        auto operator new (unsigned, void* p) -> void* { return p; }

        auto id () const { return pool.idOf(this); }

        static auto at (uint8_t i) -> Fiber& { return *(Fiber*) pool[i]; }
        static void runLoop ();
        static void suspend (Queue&, uint16_t ms =60'000);
        static void resume (uint8_t fid) { ready.append(fid); }

        static uint8_t curr;
        static Queue ready;
        static void (*app)();

        uint16_t _timeout;
        jmp_buf _context;
        uint32_t _data [];
    };

    struct Semaphore {
        Semaphore (int n =0) : count (n) {}

        void post () { if (++count <= 0) Fiber::resume(queue.pull()); }
        void pend () { if (--count < 0) Fiber::suspend(queue); }

        int16_t count;
        Queue queue;
    };
}
