struct Tcp : Ip4 {
    Net16 _sPort, _dPort;
    Net32 _seq, _ack;
    uint8_t _off, _code;
    Net16 _win, _sum, _urg;
    uint8_t _data [];

    enum : uint8_t { FIN=1<<0,SYN=1<<1,RST=1<<2,PSH=1<<3,ACK=1<<4,URG=1<<5 };
    enum : uint8_t { FREE,LSTN,SYNR,SYNS,ESTB,FIN1,FIN2,CSNG,TIMW,CLOW,LACK };

    Tcp () : Ip4 (6) {}

    static auto decode (int v, char const* s) {
        memset(smallBuf, 0, sizeof smallBuf);
        for (int i = 0; s[i] != 0; ++i)
            smallBuf[i] = v & (1<<i) ? s[i] : '.';
        return smallBuf;
    }

    struct Session : Peer {
        uint32_t lUna, rAck;
        uint16_t rIni;
        uint16_t sent;
        uint8_t state;
        ByteVec iBuf, oBuf;

        void init (Peer const& p, uint8_t s) {
            *(Peer*) this = p;
            rIni = 0;
            sent = 0;
            state = s;
            iBuf.adj(1000); oBuf.adj(1000); // set capacities
            oBuf.insert(0, 6); memcpy(oBuf.begin(), "hello\n", 6); // test data
        }

        void deinit () {
            state = FREE;
            iBuf.clear();
            oBuf.clear();
        }

        void dump (Tcp const& pkt) const {
            uint16_t rAdv = pkt._seq - rAck; if (rAdv > 99) rAdv = 99;
            uint16_t lAdv = pkt._ack - lUna; if (lAdv > 99) lAdv = 99;
            printf("  %c %23s    r %04x+%02d l %04x+%02d iBuf %d oBuf %d sent %d\n",
                    "FLRSE12GTCK"[state], "",
                    (uint16_t) (rAck-rIni), rAdv,
                    (uint16_t) (lUna-0x0400), lAdv,
                    iBuf.size(), oBuf.size(), sent);
        }
    };

    void sendReply (Interface& ni, Session& ts, uint8_t state, uint8_t flags, uint16_t bytes) {
        ts.state = state;
        auto win = ts.iBuf.cap() - ts.iBuf.size();
        printf("  > %s len %d win %d\n",
                decode(flags, "FSRPAU"), bytes, (int) win);
        Ip4::isReply(ni);
        swap(_sPort, _dPort);
        _seq = ts.lUna - bytes + ts.sent;
        _ack = flags & ACK ? ts.rAck : 0;
        _off = 5<<4;
        _code = flags;
        _win = win;
        if (true) { // calculate checksum
            _sum = 0;
            int n = bytes;
            if (n & 1)
                _data[n++] = 0;
            // TODO assumes hlen is 5, i.e. no options
            _sum = ~ check16(&_srcIp, n + 28, _proto + 20 + bytes);
        }
        sendIt(ni, sizeof *this + bytes);
    }

    void process (Interface& ni, Session& ts) {
        if (_code & RST)
            return ts.deinit(); // TODO need to reply with ACK in some cases

        // setup
        switch (ts.state) {
            case LSTN:
                if (_code & SYN) {
                    ts.lUna = 1024; // TODO use cycle counter, systick, or RNG
                    ts.rIni = _seq;
                    ts.rAck = _seq+1;
                    return sendReply(ni, ts, SYNR, SYN+ACK, 0);
                }
                return;
            case SYNR:
                if ((_code & ACK) && _ack - ts.lUna >= 1) {
                    ++ts.lUna;
                    ts.state = ESTB;
                    break;
                }
                return;
            case SYNS: // TODO
                return;
        }

        // can receive
        uint16_t nIn = _total - 4*(_off>>4) - 20;
        switch (ts.state) {
            case ESTB:
            case FIN1:
            case FIN2:
                //nIn -= ts.rAck - _seq;
                if (nIn > 0) {
                    dumpHex(_data, nIn);
                    ts.iBuf.insert(ts.iBuf.size(), nIn);
                    memcpy(ts.iBuf.end() - nIn, _data, nIn);
                }
                ts.rAck = _seq + nIn;
        }

        // can send
        uint16_t nOut = 0;
        switch (ts.state) {
            case ESTB:
            case CLOW:
                if (_ack > ts.lUna) {
                    ts.oBuf.remove(0, _ack - ts.lUna);
                    ts.sent -= _ack - ts.lUna;
                    ts.lUna = _ack;
                }
                nOut = ts.oBuf.size() - ts.sent;
                if (nOut > _win)
                    nOut = _win;
                if (nOut > 0) {
                    memcpy(_data, ts.oBuf.begin() + ts.sent, nOut);
                    ts.sent += nOut;
                }
        }

        // send done states
        switch (ts.state) {
            case FIN1:
                if ((_code & FIN) && (_code & ACK)) {
                    ++ts.rAck;
                    ++ts.lUna;
                    return sendReply(ni, ts, TIMW, ACK, 0);
                }
                if (_code & FIN) {
                    ++ts.rAck;
                    return sendReply(ni, ts, CSNG, ACK, 0);
                }
                if (_code & ACK) {
                    ++ts.lUna;
                    return sendReply(ni, ts, FIN2, ACK, 0);
                }
                return;
            case FIN2:
                if (_code & FIN) {
                    ++ts.rAck;
                    return sendReply(ni, ts, TIMW, ACK, 0);
                }
                if (ts.rAck != _seq)
                    return sendReply(ni, ts, FIN2, ACK, 0);
                return;
            case CSNG:
                if (_code & ACK) {
                    ++ts.lUna;
                    return sendReply(ni, ts, TIMW, ACK, 0);
                }
                return;
            case TIMW:
                // TODO 2*MSL timer 
                ts.deinit();
                return;
        }

        // recv done states
        switch (ts.state) {
            case CLOW:
                if (ts.oBuf.size() == 0)
                    return sendReply(ni, ts, LACK, FIN+ACK, nOut);
                if (ts.rAck != _seq || ts.lUna != _ack || nOut > 0)
                    return sendReply(ni, ts, CLOW, ACK, nOut);
                return;
            case LACK:
                if (_code & ACK) {
                    ++ts.lUna;
                    ts.deinit();
                }
                return;
        }

        // only ESTB remains
        switch (ts.state) {
            case ESTB: {
                uint8_t r = 0;
                if (_code & FIN)
                    ++ts.rAck;
                if (ts.oBuf.size() == 0)
                    r |= FIN;
                if (r != 0 || ts.rAck != _seq || ts.lUna != _ack || nOut > 0) {
                    r |= ACK;
                    return sendReply(ni, ts, _code & FIN ? FIN1 :
                                                r & FIN ? FIN1 : ESTB, r, nOut);
                }
                return;
            }
        }
    }

    void received (Interface& ni) {
#if 0
        printf("TCP %s .%d:%d -> :%d  seq %04x  ack %04x total %d @%03d\n",
                decode(_code, "FSRPAU"), (uint8_t) _srcIp,
                (int) _sPort, (int) _dPort, (uint16_t) _seq, (uint16_t) _ack,
                (int) _total, millis() % 1000);
#else
        printf("\nTCP %s .%d:%d %08x  tot %d\n",
                decode(_code, "FSRPAU"), (uint8_t) _srcIp,
                (int) _sPort, (uint32_t) _seq, (int) _total);
#endif

        auto sess = findSession({_srcIp, _sPort, _dPort});
        if (sess != nullptr) {
            sess->dump(*this);
            process(ni, *sess);
            sess->dump(*this);
        }
    }

    auto findSession (Peer const& peer) -> Session* {
        for (auto& e : sessions)
            if (e.state != FREE && e == peer)
                return &e;
        if ((_code & SYN) && listeners.exists(_dPort)) {
            for (auto& e : sessions)
                if (e.state == FREE) {
                    e.init(peer, LSTN);
                    return &e;
                }
        }
        return nullptr;
    }

    static Tcp::Session sessions [10];
};
static_assert(sizeof (Tcp) == 54);
