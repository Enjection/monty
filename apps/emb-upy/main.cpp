#include <monty.h>
#include "os.h"
#include <cassert>
#include <cstdarg>
#include <cstdlib>

using namespace monty;

int printf(char const* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto a1 = va_arg(ap, int);
    auto a2 = va_arg(ap, int);
    auto a3 = va_arg(ap, int);
    auto a4 = va_arg(ap, int);
    auto a5 = va_arg(ap, int);
    auto a6 = va_arg(ap, int);
    auto a7 = va_arg(ap, int);
    auto a8 = va_arg(ap, int);
    auto a9 = va_arg(ap, int);
    va_end(ap);

    return OS.printf(fmt, a1, a2, a3, a4, a5, a6, a7, a8, a9);
}

extern "C" int puts (char const* s) { return printf("%s\n", s); }
extern "C" int putchar (int ch) { return printf("%c", ch); }

extern "C" void __assert_func (char const* f, int l, char const* n, char const* e) {
    printf("\nassert(%s) in %s\n\t%s:%d\n", e, n, f, l);
    abort();
}

extern "C" void __assert (char const* f, int l, char const* e) {
    __assert_func(f, l, "-", e);
}

static auto loadFile (char const*) -> uint8_t const* {
    auto data = (uint8_t const*) 0x2001'F000;
    if (data[0] != 0x4D || data[1] != 0x05)
        return nullptr;
    return data;
}

auto monty::vmImport (char const* name) -> uint8_t const* {
    return loadFile(name);
}

void appMain () {
    uint8_t mem [12*1024];
    vecInit(mem, sizeof mem);
    objInit(mem, sizeof mem);
    printf("main\n");

    auto task = vmLaunch(loadFile("?")); // TODO clitask
    if (task != nullptr)
        Context::ready.append(task);
    else
        printf("no task\n");

    while (Context::runLoop())
        ; // TODO idling

    printf("done\n");
}
