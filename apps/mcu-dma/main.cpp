#include <mcu.h>

#include <cstdarg>
#include <printer.h>

using namespace mcu;

void mcu::failAt (void const* pc, void const* lr) {
    printf("failAt %p %p\n", pc, lr);
    for (uint32_t i = 0; i < systemClock() >> 15; ++i) {}
    systemReset();
}

static void duff (void* dst, void const* src, size_t len) {
    //assert(((uintptr_t) dst & 3) == 0);
    //assert(((uintptr_t) src & 3) == 0);
    //assert((len & 3) == 0);
#if 0
    memcpy(dst, src, len);
#else
    // see https://en.wikipedia.org/wiki/Duff%27s_device
    auto to = (uint32_t*) dst;
    auto from = (uint32_t const*) src;
    auto count = len/4;
    auto n = (count + 7) / 8;
    switch (count % 8) {
        case 0: do { *to++ = *from++; [[fallthrough]];
        case 7:      *to++ = *from++; [[fallthrough]];
        case 6:      *to++ = *from++; [[fallthrough]];
        case 5:      *to++ = *from++; [[fallthrough]];
        case 4:      *to++ = *from++; [[fallthrough]];
        case 3:      *to++ = *from++; [[fallthrough]];
        case 2:      *to++ = *from++; [[fallthrough]];
        case 1:      *to++ = *from++;
                } while (--n > 0);
    }
#endif
}

constexpr auto N = 4096;
uint32_t buf1 [N], buf2 [N];

namespace mcu {
    struct MemDma : Device {
        constexpr static auto DMA = io32<0x4002'0400>; // DMA2
        enum { ISR=0x00,IFCR=0x04,CCR=0x08,CNDTR=0x0C,CPAR=0x10,CMAR=0x14,CSELR=0xA8 };

        void init () {
            constexpr auto AHB1ENR = 0x48;
            RCC(AHB1ENR)[1] = 1; // DMA2EN on
            DMA(CSELR) = 0; // reset value
            DMA(CCR) = // MEM2MEM MSIZE PSIZE MINC PINC TCIE
                (1<<14) | (3<<12) | (2<<10) | (2<<8) | (1<<7) | (1<<6) | (1<<1);
            irqInstall((uint8_t) IrqVec::DMA2_CH1);
        }

        void xfer () {
            DMA(CCR)[0] = 0; // ~EN
            DMA(CNDTR) = N;
            DMA(CPAR) = (uint32_t) buf2;
            DMA(CMAR) = (uint32_t) buf1;
            DMA(CSELR) = 0; // reset value
            DMA(CCR)[0] = 1; // EN
            asm ("wfi");
        }

        void irqHandler () override {
            DMA(IFCR)[0] = 1;
            DMA(CCR)[0] = 0; // ~EN
        }
    };
}

int main () {
    fastClock();
    msWait(500);

    Serial<Uart> serial (2, "A2:PU7,A15:PU3");

    Printer printer (&serial, [](void* obj, uint8_t const* ptr, int len) {
        ((Serial<Uart>*) obj)->write(ptr, len);
    });
    stdOut = &printer;

    // output:
    //
    //  msWait:  80122 cycles, 1001 µs (1-1)
    //  memcpy: 114711 cycles, 1433 µs (2-2)
    //  duff's:  11301 cycles,  141 µs (3-3)
    //  memDma:  20725 cycles,  259 µs (4-4)
    //  inline:  28685 cycles,  358 µs (5-5)

    auto hz = mcu::systemClock(), mhz = hz/1'000'000;
    serial.baud(921600, hz);
    printf("%d MHz\n", mhz);

    buf1[0] = 1; buf2[0] = 0; buf1[N-1] = -1; buf2[N-1] = 0;
    cycles::start();
    auto t = cycles::count();
    msWait(1);
    t = cycles::count() - t;
    printf("msWait: %6d cycles, %4d µs (%d%d)\n", t, t/mhz, buf1[0], buf1[N-1]);
    msWait(50);

    buf1[0] = 0; buf2[0] = 2; buf1[N-1] = 0; buf2[N-1] = -2;
    t = cycles::count();
    memcpy(buf1, buf2, sizeof buf1);
    t = cycles::count() - t;
    printf("memcpy: %6d cycles, %4d µs (%d%d)\n", t, t/mhz, buf1[0], buf1[N-1]);
    msWait(50);

    buf1[0] = 0; buf2[0] = 3; buf1[N-1] = 0; buf2[N-1] = -3;
    t = cycles::count();
    duff(buf1, buf2, sizeof buf1);
    t = cycles::count() - t;
    printf("duff's: %6d cycles, %4d µs (%d%d)\n", t, t/mhz, buf1[0], buf1[N-1]);
    msWait(50);

    buf1[0] = 0; buf2[0] = 4; buf1[N-1] = 0; buf2[N-1] = -4;
    t = cycles::count();
    MemDma memDma;
    memDma.init();
    memDma.xfer();
    t = cycles::count() - t;
    printf("memDma: %6d cycles, %4d µs (%d%d)\n", t, t/mhz, buf1[0], buf1[N-1]);
    msWait(50);

    buf1[0] = 0; buf2[0] = 5; buf1[N-1] = 0; buf2[N-1] = -5;
    t = cycles::count();
    for (int i = 0; i < N; ++i)
        buf1[i] = buf2[i];
    t = cycles::count() - t;
    printf("inline: %6d cycles, %4d µs (%d%d)\n", t, t/mhz, buf1[0], buf1[N-1]);
    msWait(50);

    while (true) {
        //msWait(100);
        printf("%d\n", millis());
        auto [ptr, len] = serial.recv();
        printf("%d %02x\n", len, *ptr);
        serial.didRecv(1);
    }
}
