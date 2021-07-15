#if BIOS_INIT || BIOS_MAIN

#include <cstdarg>

struct Bios {
    virtual int vprintf (char const*, va_list) const;
    virtual void led (int) const;
    virtual unsigned now () const;
    virtual void delay (unsigned) const;
    virtual int getch () const;
};

struct App {
    constexpr static auto MAGIC = 0x12340000;
    int magic;
    int (*init)();
    void* ebss;
    Bios const* bios;
};

#endif // BIOS_INIT || BIOS_MAIN

#if BIOS_INIT

extern int _sbss [], _ebss [];

static int appInit () {
    for (auto p = _sbss; p != _ebss; ++p)
        *p = 0; // clear BSS

    extern void (*__init_array_start[])(), (*__init_array_end[])();
    for (auto p = __init_array_start; p != __init_array_end; ++p)
        (*p)(); // call constructors

    extern int main ();
    return main();
}

__attribute__ ((section(".start")))
App app { App::MAGIC, appInit, _ebss, nullptr };

namespace bios {
    void led (int on) { app.bios->led(on); }
    unsigned now () { return app.bios->now(); }
    void delay (unsigned ms) { app.bios->delay(ms); }
    int getch () { return app.bios->getch(); }
}

extern "C" int printf (char const* fmt ...) {
    va_list ap;
    va_start(ap, fmt);
    auto r = app.bios->vprintf(fmt, ap);
    va_end(ap);
    return r;
}

#else

namespace bios {
    void led (int);
    unsigned now ();
    void delay (unsigned);
    int getch ();
}

extern "C" int printf (char const* ...);

#endif // BIOS_INIT
