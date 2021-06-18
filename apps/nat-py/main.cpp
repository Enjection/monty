#include <monty.h>
#include <cstdio>

using namespace monty;

int main (int argc, char const** argv) {
    uint8_t mem [12*1024];
    vecInit(mem, sizeof mem);
    objInit(mem, sizeof mem);
    printf("main\n");

    auto task = argc > 1 ? vmLaunch(argv[1]) : nullptr;
    if (task != nullptr)
        Context::ready.append(task);
    else
        printf("no task\n");

    while (Context::runLoop())
        ; // TODO

    printf("done\n");
}
