// TODO messy code, mostly ad-hoc, many cases untested, bug's paradise ...

struct Parser {
    enum State : uint8_t { Ini, Cmd, Hex, Obj, Err,
            IHEX, SKIP, END, STR, ESC, STRX, STRU, NUM, WORD, ARGS };

    auto feed (uint8_t const* ptr, int len) {
        int pos = 0;
        while (pos < len) {
            auto b = ptr[pos++];
            if (b == '\r' || (b < ' ' && b != '\n'))
                continue;
            state = next(b);
            if (Ini < state && state <= Err)
                break; // parse complete
        }
        return pos;
    }

    auto hexSize () const { return buf[0]; }
    auto hexAddr () const { return 256*buf[1] + buf[2]; }
    auto hexType () const { return buf[3]; }
    auto hexData () const { return buf+4; }

    Value val; // TODO mark for GC!
    State state =Ini;
private:
    uint8_t tag, fill, chk;
    uint8_t buf [37]; // len:1, addr:2, type:1, data:0..32, chk:1
    uint64_t u64;
    Vector stack; // TODO mark for GC!

    auto next (uint8_t b) -> State {
        switch (state) {
            case Err:
            case SKIP:
                if (b != '\n')
                    return SKIP;
                [[fallthrough]];
            case Ini:
            case Cmd:
            case Hex:
            case Obj:
                state = Ini;
                val = {};
                fill = chk = 0;
                tag = b;
                switch (b) {
                    case '\n':  return Ini;
                    case ':':   return IHEX;
                    case '_':
                    case '(':
                    case '[':
                    case '{':   stack.append(b);
                                stack.append(new List);
                                return Ini;
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
                                return Err;
                }
                break;

            case IHEX:
                if (int n = fill/2; b == '\n')
                    return n > 0 && n == buf[0]+5 && chk == 0 ? Hex : Err;
                else if (n < (int) sizeof buf) {
                    // ignore malformed data, but avoid buffer overruns
                    buf[n] = 16*buf[n] + ((b&0x40 ? b+9 : b) & 0x0F);
                    if (fill & 1)
                        chk += buf[n];
                    ++fill;
                }
                return IHEX;

            case STR:
                if (b == tag) {
                    if (tag == '"')
                        addByte(0, false);
                    return END;
                }
                if (b == '\\')
                    return ESC;
                addByte(b);
                return STR;

            case ESC:
                switch (b) {
                    case 'b': b = '\b'; break;
                    case 'f': b = '\f'; break;
                    case 'n': b = '\n'; break;
                    case 'r': b = '\r'; break;
                    case 't': b = '\t'; break;
                    case 'u': chk = 4; return STRU;
                    case 'x': chk = 2; return STRX;
                }
                addByte(b);
                return STR;

            case STRX:
            case STRU: // ignore malformed hex
                u64 = ((uint16_t) u64 << 4) + ((b & 0x40 ? b + 9 : b) & 0x0F);
                if (--chk > 0)
                    return state;
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

            case NUM:
                if ('0' <= b && b <= '9') {
                    u64 = 10 * u64 + (b - '0');
                    return NUM;
                }
                val = Int::make(tag == '-' ? -u64 : u64);
                break;

            case WORD:
                if (fill >= sizeof buf - 2)
                    return Err;
                // exclude JSON separators, but allow - . / @ _ and  a few more
                if (('-'<=b && b<='9') || ('@'<=b && b<='Z') || ('_'<=b && b<='z')) {
                    buf[fill++] = b;
                    break;
                }
                buf[fill] = 0;
                val = strcmp((char*) buf, "null" ) == 0 ? Null  :
                      strcmp((char*) buf, "false") == 0 ? False :
                      strcmp((char*) buf, "true" ) == 0 ? True  : Value ();
                if (!val.isNil())
                    break;
                [[fallthrough]];
            case ARGS:
                if (b == '\n') {
                    val = new Str ((char const*) buf, fill);
                    return Cmd;
                }
                if (b < ' ' || b > '~' || fill >= sizeof buf - 2)
                    return Err;
                buf[fill++] = b;
                buf[fill] = 0;
                return ARGS;

            case END:
                break;
        }

        if (b == '\n') {
            stack.clear();
            return Obj;
        }

        if (stack.size() == 0)
            return state;

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
                        return Err;
                }
                return END;
            }
            default:
                return Err;
        }

        return Ini;
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
