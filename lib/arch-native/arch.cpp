#include <monty.h>
#include "arch.h"

#include <extend.h>

#include <cassert>
#include <cstdio>
#include <ctime>

static void* pool;

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
    char buf [40];
    sprintf(buf, "pytests/%s.mpy", name);
    return loadFile(buf);
}

void arch::init (int size) {
    setbuf(stdout, nullptr);
    if (size <= 0)
        size = 1024*1024; // allocate a whopping 1 Mb
    pool = malloc(size);
    monty::gcSetup(pool, size);
    monty::Event::triggers.append(0); // TODO yuck, reserve 1st entry for VM
}

void arch::idle () {
    timespec ts { 0, 100000 };
    nanosleep(&ts, &ts); // 100 µs, i.e. 10% of ticks' 1 ms resolution
}

auto arch::done () -> int {
    //free(pool);
    return 0;
}
