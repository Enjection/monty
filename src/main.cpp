#include <monty.h>
#include <arch.h>

#if DOCTEST
#include <doctest.h>
#endif

#if 0
#include <cstdio>

void boss::debugf (const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}
#endif

using namespace monty;

#ifdef MAIN
MAIN
#else
int main ([[maybe_unused]] int argc, [[maybe_unused]] char const** argv)
#endif
{
#if DOCTEST
    // must run before arch::init, because gcSetup is called in several tests
    if (auto r = doctest::Context(argc, argv).run(); r != 0)
        return r;
#endif

#if STM32L053xx
    arch::init(3*1024); // there's only 8 kB RAM ...
#else
    arch::init(12*1024);
#endif
#ifndef NDEBUG
    printf("main\n");
#endif

#if NATIVE
    auto task = argc > 1 ? vmLaunch(argv[1]) : nullptr;
#else
    auto task = arch::cliTask();
#endif
    if (task != nullptr)
        Context::ready.append(task);
    else
        printf("no task\n");

    while (Context::runLoop())
        arch::idle();

#ifndef NDEBUG
    printf("done\n");
#endif
    arch::done();
}
