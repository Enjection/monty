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

namespace boss::pool {
    using Id_t = uint8_t;
    extern uint8_t numFree;

    void init (void* ptr, size_t len);
    void deinit ();

    struct Buf {
        auto operator new (size_t bytes) -> void*;
        void operator delete (void*);

        auto id () const { return asId(this); }
        auto tag () -> Id_t&;

        static auto asId (void const*) -> Id_t;
        static auto at (Id_t) -> Buf&;

        uint8_t data [512];
    };

    struct Queue {
        bool isEmpty () const { return head == 0; }

        auto pull () -> Id_t;
        void insert (Id_t id);
        void insert (void* p) { insert(Buf::asId(p)); }
        void append (Id_t id);
        void append (void* p) { append(Buf::asId(p)); }

        template <typename F>
        auto remove (F f) {
            Queue q;
            auto p = &head;
            while (*p != 0) {
                auto id = *p;
                auto& next = tag(id);
                if (f(id)) {
                    if (tail == id)
                        tail = p == &head ? 0 : p - &tag(0); // TODO yuck
                    *p = next;
                    q.append(id);
                } else
                    p = &next;
            }
            return q;
        }

        void verify () const;
    private:
        auto tag (Id_t id) const -> Id_t& { return Buf::at(id).tag(); }

        Id_t head {0}, tail {0};
    };
}

namespace boss {
    auto veprintf(void(*)(void*,int), void*, char const* fmt, va_list ap) -> int;
    void debugf (const char* fmt, ...);
    [[noreturn]] void failAt (void const*, void const*);

    struct Fiber {
        using Fid_t = pool::Id_t;

        static auto expire (pool::Queue&, uint16_t now, uint16_t& limit) -> int;

        auto id () const { return pool::Buf::asId(this); }
        void resume (int i) { result = i; ready.append(id()); }

        static auto at (Fid_t i) -> Fiber& { return (Fiber&) pool::Buf::at(i); }
        static auto runLoop () -> bool;
        static auto create (void (*)(void*), void* =nullptr) -> Fid_t;
        static void processPending ();
        static void msWait (uint16_t ms) { suspend(timers, ms); }
        static auto suspend (pool::Queue&, uint16_t ms =60'000) -> int;
        static void duff (uint32_t* dst, uint32_t const* src, uint32_t cnt);

        static Fid_t curr;
        static pool::Queue ready;
        static pool::Queue timers;

        int8_t result;
        uint16_t timeout;
        union {
            jmp_buf context;
            struct { void (*fun)(void*); void* arg; };
        };
        uint32_t data [];
    };

    struct Semaphore : private pool::Queue {
        Semaphore (int n) : count (n) {}

        void post () {
            if (++count <= 0)
                Fiber::at(pull()).resume(1);
        }

        auto pend (uint32_t ms =60'000) -> int {
            return --count >= 0 ? 1 : Fiber::suspend(*this, ms);
        }

        void expire (uint16_t now, uint16_t& limit);
    private:
        int16_t count;
    };
}
