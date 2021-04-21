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
    enum State : uint8_t {
        Ini, Cmd, Hex, Obj, Err,
        IHEX, SKIP, END, STR, ESC, STRX, STRU, NUM, WORD, ARGS,
    };

    InputParser () {}

    auto feed (uint8_t const* ptr, int len) {
        int pos = 0;
        while (pos < len) {
            state = next(ptr[pos++]);
            if (Ini < state && state <= Err)
                break; // parse complete
        }
        return pos;
    }

    auto hexSize () const { return buf[0]; }
    auto hexAddr () const { return 256*buf[1] + buf[2]; }
    auto hexType () const { return buf[3]; }
    auto hexData () const { return buf+4; }

    State state =Ini;
    Value val;
private:
    uint8_t last =0, tag, fill, sum;
    uint8_t buf [37]; // len:1, addr:2, type:1, data:0..32, sum:1
    uint64_t u64;
    Vector stack;

    auto next (uint8_t b) -> State {
        auto crlf = b == '\n' && last == '\r';
//printf("%c %02x %d %d\n", b >= ' ' ? b : '.', b, state, crlf);
        last = b;
        if (crlf)
            return Ini;
        if (b == '\r')
            b = '\n';
        switch (state) {
            case SKIP:
                if (b != '\n')
                    break;
                [[fallthrough]];
            case Ini:
            case Cmd:
            case Hex:
            case Obj:
            case Err:
                state = Ini;
                tag = b;
                switch (b) {
                    case ':':   fill = sum = 0; return IHEX;
                    case '_':
                    case '(':
                    case '[':
                    case '{':   stack.append(b);
                                stack.append(new List);
                                return state;
                    case ')':
                    case ']':
                    case '}':   break;
                    case '\'':  val = new Bytes;
                                return STR;
                    case '"':   val = new Str ("");
                                return STR;
                    case '-':   b = '0';
                                [[fallthrough]];
                    default:    if ('0' <= b && b <= '9') {
                                    u64 = b - '0';
                                    return NUM;
                                }
                                if (('A'<=b && b<='Z') || ('a'<=b && b<='z')) {
                                    fill = 0;
                                    buf[fill++] = b;
                                    return WORD;
                                }
                                return SKIP; // ignore until next newline
                }
                break;

            case IHEX:
                if (int n = fill/2; b == '\n')
                    return n > 0 && n == buf[0]+5 && sum == 0 ? Hex : Err;
                else if (n < (int) sizeof buf) {
                    // ignore malformed data, but avoid buffer overruns
                    buf[n] = 16*buf[n] + ((b&0x40 ? b+9 : b) & 0x0F);
                    if (fill & 1)
                        sum += buf[n];
                    ++fill;
                }
                break;

            case NUM:
                if ('0' <= b && b <= '9') {
                    u64 = 10 * u64 + (b - '0');
                    return state;
                }
                val = Int::make(tag == '-' ? -u64 : u64);
                break;

            case STR:
                if (b == tag) {
                    if (tag == '"')
                        addByte(0, false);
                    return END;
                }
                if (b == '\\')
                    return ESC;
                addByte(b);
                return state;

            case ESC:
                switch (b) {
                    case 'b': b = '\b'; break;
                    case 'f': b = '\f'; break;
                    case 'n': b = '\n'; break;
                    case 'r': b = '\r'; break;
                    case 't': b = '\t'; break;
                    case 'u': fill = 4; return STRU;
                    case 'x': fill = 2; return STRX;
                }
                addByte(b);
                return STR;

            case STRX:
            case STRU: { // ignore malformed hex
                u64 = ((uint16_t) u64 << 4) + ((b & 0x40 ? b + 9 : b) & 0x0F);
                if (--fill == 0) {
                    if (state == STRX)
                        addByte(u64);
                    else {
                        uint16_t u = u64;
                        if (u > 0x7F) {
                            if (u <= 0x7FF)
                                addByte(0xC0 | (u>>6));
                            else {
                                addByte(0xE0 | (u>>12));
                                addByte(0x80 | ((u>>6) & 0x3F));
                            }
                            u = 0x80 | (u & 0x3F);
                        }
                        addByte(u);
                    }
                    return STR;
                }
                return state;
            }

            case END:
                break;

            case WORD:
                if ('a' <= b && b <= 'z' && fill < sizeof buf - 2) {
                    buf[fill++] = b;
                    break;
                }
                buf[fill] = 0;
                val = strcmp((char*) buf, "null") == 0 ? Null :
                      strcmp((char*) buf, "false") == 0 ? False :
                      strcmp((char*) buf, "true") == 0 ? True : Value ();
                if (val.isNil())
                    return ARGS;
                break;

            case ARGS:
                if (b < ' ' || b > '~' || fill >= sizeof buf - 2)
                    return Err;
                buf[fill++] = b;
                buf[fill] = 0;
                break;
        }

        if (b == '\n') {
            stack.clear();
            return Obj;
        }

if (stack.size() == 0) return state;
        auto& v = stack.pop().asType<List>();
        if (!val.isNil())
            v.append(val);

        switch (b) {
            case ':':
                stack.end()[-1] = b; // mark as dict, not a set
                [[fallthrough]];
            case ',':
                stack.append(v);
                break;
            case '_':
                tag = stack.end()[-1];
                if (v.size() == 0 && tag == '{')
                    stack.end()[-1] = b; // mark as empty set
                stack.append(v);
                break;
            case ')':
            case ']':
            case '}': {
                val = v;
                tag = stack.pop();
                switch (tag) {
                    case '(':
                        val = Tuple::create({v, (int) v.size(), 0});
                    case '[':
                        break;
                    case '_':
                    case '{':
                        if (tag == '_' || v.size() > 0) {
                            val = Set::create({v, (int) v.size(), 0});
                            break;
                        }
                        [[fallthrough]];
                    case ':':
                        if (v.size() & 1)
                            val = {}; // ignore malformed dict
                        else {
                            auto p = new Dict;
                            p->adj(v.size());
                            for (uint32_t i = 0; i < v.size(); i += 2)
                                p->at(v[i]) = v[i+1]; // TODO avoid lookups
                            val = p;
                        }
                        break;
                    default:
                        assert(false);
                }
                return END;
            }
            default:
printf("fail b <%c> s %d\n", b, state);
                assert(false);
        }

        return state;
    }

    void addByte (uint8_t b, bool extend =true) {
        auto& v = (Bytes&) val.obj(); // also deals with derived Str type
        auto n = v.size();
        if (extend)
            v.insert(n);
        else
            v.adj(n+1);
        v[n] = b;
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
                        printf("CMD\n");
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
            dumpHex(buf, n);
        }
    }

    arch::done();
}
