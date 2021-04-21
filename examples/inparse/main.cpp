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

struct InputParser {
    enum State : uint8_t { Ini, Cmd, Hex, Obj, Err };
    State state =Ini;

    InputParser () {}

    auto feed (uint8_t const* ptr, int len) {
        int pos = 0;
        while (pos < len) {
            state = next(ptr[pos++]);
            if (Ini < state && state <= Err)
                break; // parse complete
        }
        return len;
    }
private:
    uint8_t tag;

    auto next (uint8_t b) -> State {
        switch (state) {
            case Ini:
            case Cmd:
            case Hex:
            case Obj:
            case Err:
                tag = b;
                switch (b) {
                }
                break;
        }
        return state;
    }
};

int main (int argc, char const** argv) {
    arch::init(10*1024);

    assert(argc == 2);
    FILE* fp = fopen(argv[1], "r");
    if (fp == nullptr)
        perror(argv[1]);
    else {
        InputParser parser;

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
                        printf("cmd\n");
                        break;
                    case parser.Hex:
                        printf("hex\n");
                        break;
                    case parser.Obj:
                        printf("obj\n");
                        break;
                    case parser.Err:
                        printf("err\n");
                        break;
                    default: break;
                }
            }
            dumpHex(buf, n);
        }
    }

    arch::done();
}
