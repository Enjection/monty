#include <monty.h>
#include <arch.h>

using namespace monty;

#ifdef MAIN
MAIN
#else
int main ([[maybe_unused]] int argc, [[maybe_unused]] char const** argv)
#endif
{
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
        Stacklet::ready.append(task);
    else
        printf("no task\n");

    while (Stacklet::runLoop())
        arch::idle();

#ifndef NDEBUG
    printf("done\n");
#endif
    arch::done();
}
