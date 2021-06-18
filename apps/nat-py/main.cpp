#include <monty.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>

using namespace monty;

constexpr auto IMPORT_PATH = "tests/py/%s.mpy";

static auto loadFile (char const* name) -> uint8_t const* {
    auto fp = fopen(name, "rb");
    if (fp == nullptr)
        return nullptr;
    fseek(fp, 0, SEEK_END);
    uint32_t bytes = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    auto data = (uint8_t*) malloc(bytes + 64);
    fread(data, 1, bytes, fp);
    fclose(fp);
    return data;
}

auto monty::vmImport (char const* name) -> uint8_t const* {
    auto data = loadFile(name);
    if (data != nullptr)
        return data;
    assert(strlen(name) < 25);
    char buf [sizeof IMPORT_PATH + 25];
    sprintf(buf, IMPORT_PATH, name);
    return loadFile(buf);
}

int main (int argc, char const** argv) {
    uint8_t mem [12*1024];
    vecInit(mem, sizeof mem);
    objInit(mem, sizeof mem);
    printf("main\n");

    auto task = argc > 1 ? vmLaunch(argv[1]) : nullptr; // TODO clitask
    if (task != nullptr)
        Context::ready.append(task);
    else
        printf("no task\n");

    while (Context::runLoop())
        ; // TODO idling

    printf("done\n");
}
