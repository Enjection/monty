#include <hall.h>
#include <setjmp.h>

using namespace hall;

template <int N =30, int SZ =128>
struct Pool {
    enum { NBUF=N, SZBUF=SZ };

    Pool () {
        for (int i = 0; i < N-1; ++i)
            tag(i) = i+1;
        irqFree = tag(N-1) = 0;
    }

    auto idOf (void const* p) const -> uint8_t {
        //TODO ensure(buffers[1] <= p && p < buffers[N]);
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
        //TODO ensure(n != 0);
if (n == 0) while (true) {}
        tag(0) = tag(n);
        tag(n) = 0;
        return buffers[n];
    }

    void release (uint8_t i) { tag(i) = tag(0); tag(0) = i; }
    void releasePtr (uint8_t* p) { release(idOf(p)); }

    // only call this version from interrupt context, i.e. with IRQs disabled
    void irqRelease (uint8_t i) { tag(i) = irqFree; irqFree = i; }
    void irqReleasePtr (uint8_t* p) { irqRelease(idOf(p)); }

    auto numFree () const {
        int n = 0;
        for (int i = tag(0); i != 0; i = tag(i))
            ++n;
        BlockIRQ crit;
        for (int i = irqFree; i != 0; i = tag(i))
            ++n;
        return n;
    }

    void check () const {
        bool inUse [N];
        memset(inUse, 0, sizeof inUse);
        for (int i = tag(0); i != 0; i = tag(i)) {
            //TODO ensure(!inUse[i]);
            inUse[i] = true;
        }
        BlockIRQ crit;
        for (int i = irqFree; i != 0; i = tag(i)) {
            //TODO ensure(!inUse[i]);
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

Pool pool;

struct Queue {
    auto pull () {
        auto i = first;
        if (i != 0) {
            first = pool.tag(i);
            if (first == 0)
                last = 0;
            pool.tag(i) = 0;
        }
        return i;
    }
    void insert (uint8_t i) {
        pool.tag(i) = first;
        first = i;
        if (last == 0)
            last = first;
    }
    void append (uint8_t i) {
        if (last != 0)
            pool.tag(last) = i;
        last = i;
        if (first == 0)
            first = last;
    }

    uint8_t first =0, last =0;
};

struct Fiber {
    auto operator new (unsigned, void* p) -> void* { return p; }

    auto id () const { return pool.idOf(this); }

    static auto at (uint8_t i) -> Fiber& { return *(Fiber*) pool[i]; }

    static auto current () {
        if (curr == 0)
            curr = (new (pool.allocate()) Fiber)->id();
        return curr;
    }

    static void suspend (Queue&, uint16_t =60'000);
    static void resume (uint8_t);

    static uint8_t curr;
    static Queue ready;

    uint16_t _timeout;
    jmp_buf _context;
    uint8_t _data [];
};

uint8_t Fiber::curr;
Queue Fiber::ready;

struct Semaphore {
    Semaphore (int n =0) : count (n) {}

    void post () {
        if (++count <= 0)
            Fiber::resume(queue.pull());
    }
    void pend () {
        if (--count < 0)
            Fiber::suspend(queue);
    }

    int16_t count;
    Queue queue;
};

struct DmaInfo { uint32_t base; uint8_t streams [8]; };

DmaInfo const dmaInfo [] = {
    { 0x4002'0000, { 0, 11, 12, 13, 14, 15, 16, 17 }},
    { 0x4002'0400, { 0, 56, 57, 58, 59, 60, 68, 69 }},
};

struct UartInfo {
    uint8_t num :4, ena, irq;
    uint16_t dma :1, rxChan :4, rxStream :3, txChan :4, txStream :3;
    uint32_t base;

    auto dmaBase () const { return dmaInfo[dma].base; }
};

namespace hall {
#include <uart-stm32l4.h>
}

Uart uart [] = {
    UartInfo { 1, 78, 37, 0, 2, 5, 2, 4, 0x4001'3800 },
    UartInfo { 2, 17, 38, 0, 2, 6, 2, 7, 0x4000'4400 },
    UartInfo { 3, 18, 38, 0, 2, 3, 2, 2, 0x4000'4800 },
    UartInfo { 4, 19, 52, 1, 2, 5, 2, 3, 0x4000'4C00 },
};

#include <cstdarg>
#include <printer.h>

Printer printf (&uart[1], [](void* arg, uint8_t n) {
    ((Uart*) arg)->txStart(n);
});

int main () {
    fastClock();

    Pin leds [7];
    Pin::define("A6:P,A5,A4,A3,A1,A0,B3", leds, 7);

    systick::init(); // defaults to 100 ms
    cycles::init();

    uart[1].init("A2:PU7,A15:PU3", 921600);
    printf("\n");
    asm ("wfi");

    for (int n = 0; n < 50; ++n) {
        for (int i = 0; i < 6; ++i)
            leds[i] = n % (i+2) == 0;

        printf("hello %4d %010u\n", systick::millis(), cycles::count());
        asm ("wfi");

        leds[6] = 0;
        asm ("wfi");
        leds[6] = 1;

        Device::processAllPending();
    }
}
