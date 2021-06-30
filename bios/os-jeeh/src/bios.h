struct Bios {
    virtual void led (int) const;
    virtual void delay (unsigned) const;
    virtual int printf (char const*, ...) const;
    virtual unsigned now () const;
};

struct App {
    constexpr static auto MAGIC = 0x12340000;
    int magic;
    void (*init)();
    void* ebss;
    Bios const* bios;
};

extern __attribute__ ((section(".start"))) App app;
#define OS (*app.bios)

#if BIOS_MAIN

extern void appMain ();

extern int _sbss [], _ebss [];

static void appInit () {
    for (auto p = _sbss; p != _ebss; ++p)
        *p = 0;
#if 0
    extern void (*__preinit_array_start []) ();
    extern void (*__preinit_array_end []) ();
    for (auto p = __preinit_array_start; p != __preinit_array_end; ++p)
        (*p)();
    _init();
#endif
    extern void (*__init_array_start []) ();
    extern void (*__init_array_end []) ();
    for (auto p = __init_array_start; p != __init_array_end; ++p)
        (*p)();

    appMain();
}

App app { App::MAGIC, appInit, _ebss, nullptr };

#endif // BIOS_MAIN
