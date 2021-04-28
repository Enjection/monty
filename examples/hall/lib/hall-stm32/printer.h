struct Printer {
    Printer (void* arg, void (*fun) (void*, uint8_t)) : fun (fun), arg (arg) {}

    auto vprintf(char const* fmt, va_list ap) {
        count = 0;
        while (*fmt)
            if (char c = *fmt++; c != '%')
                emit(c);
            else {
                pad = *fmt == '0' ? '0' : ' ';
                width = radix = 0;
                while (radix == 0)
                    switch (c = *fmt++) {
                        case 'b': radix =  2; break;
                        case 'o': radix =  8; break;
                        case 'u':
                        case 'd': radix = 10; break;
                        case 'p': pad = '0'; width = 8;
                                  [[fallthrough]];
                        case 'x': radix = 16; break;
                        case 'c': putFiller(width - 1);
                                  c = va_arg(ap, int);
                                  [[fallthrough]];
                        case '%': emit(c); radix = 1; break;
                        case '*': width = va_arg(ap, int); break;
                        case 's': { char const* s = va_arg(ap, char const*);
                                    while (*s) {
                                        emit(*s++);
                                        --width;
                                    }
                                    putFiller(width);
                                  }
                                  [[fallthrough]];
                        default:  if ('0' <= c && c <= '9')
                                      width = 10 * width + c - '0';
                                  else
                                      radix = 1; // stop scanning
                    }
                if (radix > 1) {
                    static uint8_t numBuf [32]; // shared by all Printer objs
                    uint8_t numFill = sizeof numBuf;
                    int val = va_arg(ap, int);
                    auto sign = val < 0 && c == 'd';
                    uint32_t num = sign ? -val : val;
                    do {
                        numBuf[--numFill] = "0123456789ABCDEF"[num % radix];
                        num /= radix;
                    } while (num != 0);
                    if (sign) {
                        if (pad == ' ')
                            numBuf[--numFill] = '-';
                        else {
                            --width;
                            emit('-');
                        }
                    }
                    putFiller(width - (sizeof numBuf - numFill));
                    while (numFill < sizeof numBuf)
                        emit(numBuf[numFill++]);
                }
            }
        return count;
    }

    auto operator() (const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int result = vprintf(fmt, ap);
        va_end(ap);
        return result;
    }

    void (*fun) (void*, uint8_t);
    void* arg;
private:
    uint16_t count, fill =0;
    int16_t width;
    int8_t pad, radix;
    uint8_t* buf;

    void flush () {
        //TODO ensure(fill > 0);
        auto n = pool.idOf(buf);
        pool.tag(n) = fill - 1;
        //TODO ensure(pool.tag(n) == fill);
        count += fill;
        fill = 0;
        buf = nullptr;
        fun(arg, n);
    }

    void emit (int c) {
        if (fill == 0)
            buf = pool.allocate();
        else if (fill >= pool.SZBUF)
            flush();
        buf[fill++] = c;
        if (c == '\n')
            flush();
    }

    void putFiller (int n) {
        while (--n >= 0)
            emit(pad);
    }
};
