#include <monty.h>
#include <arch.h>
#include <cassert>
#include <cstdio>

using namespace monty;

#include "parser.h"

int main (int argc, char const** argv) {
    arch::init(10*1024);

    assert(argc == 2);
    FILE* fp = fopen(argv[1], "r");
    if (fp == nullptr)
        perror(argv[1]);
    else {
        Parser parser;

        uint8_t buf [16];
        while (true) {
            auto n = fread(buf, 1, sizeof buf, fp);
            if (n <= 0)
                break;
            uint8_t const* ptr = buf;
            while (n > 0) {
                int i = parser.feed(ptr, n);
                assert(0 < n && n <= n);
                ptr += i;
                n -= i;

                switch (parser.state) {
                    case parser.Cmd:
                        printf("CMD <%s>\n", (char const*) parser.val);
                        break;
                    case parser.Hex:
                        printf("HEX t %d a %04X s %d\n",
                                parser.hexType(),
                                parser.hexAddr(),
                                parser.hexSize());
                        break;
                    case parser.Obj:
                        printf("OBJ ");
                        {
                            Buffer buf;
                            buf << parser.val;
                        }
                        printf("\n");
                        break;
                    case parser.Err:
                        printf("ERR\n");
                        break;
                    default: break;
                }
            }
        }
    }

    arch::done();
}
