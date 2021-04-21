#include <monty.h>
#include <arch.h>
#include <cassert>
#include <cstdio>

using namespace monty;

void dumpHex (void const* p, int n =16) {
    for (int off = 0; off < n; off += 16) {
        printf(" %03x:", off);
        for (int i = 0; i < 16; ++i) {
            if (i % 4 == 0)
                printf(" ");
            if (off+i >= n)
                printf("..");
            else {
                auto b = ((uint8_t const*) p)[off+i];
                printf("%02x", b);
            }
        }
        for (int i = 0; i < 16; ++i) {
            if (i % 4 == 0)
                printf(" ");
            auto b = ((uint8_t const*) p)[off+i];
            printf("%c", off+i >= n ? ' ' : ' ' <= b && b <= '~' ? b : '.');
        }
        printf("\n");
    }
}

int main (int argc, char const** argv) {
    arch::init(10*1024);

    assert(argc == 2);
    FILE* fp = fopen(argv[1], "r");
    if (fp == nullptr)
        perror(argv[1]);
    else {
        char buf [16];
        while (true) {
            auto n = fread(buf, 1, sizeof buf, fp);
            if (n <= 0)
                break;
            dumpHex(buf, n);
        }
    }

    arch::done();
}
