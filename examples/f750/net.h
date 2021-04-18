template <typename T>
void swap (T& a, T& b) { T t = a; a = b; b = t; }

struct MacAddr {
    uint8_t b [6];

    auto operator== (MacAddr const& v) const {
        return memcmp(this, &v, sizeof *this) == 0;
    }

    auto asStr (SmallBuf& buf =smallBuf) const {
        auto ptr = buf;
        for (int i = 0; i < 6; ++i)
            ptr += snprintf(ptr, 4, "%c%02x", i == 0 ? ' ' : ':', b[i]);
        return buf + 1;
    }
};

struct Net16 {
    constexpr Net16 (uint16_t v =0) : h ((v<<8) | (v>>8)) {}
    operator uint16_t () const { return (h<<8) | (h>>8); }
private:
    uint16_t h;
};

struct Net32 {
    constexpr Net32 (uint32_t v =0) : b2 {(uint16_t) (v>>16), (uint16_t) v} {}
    operator uint32_t () const { return (b2[0]<<16) | b2[1]; }
private:
    Net16 b2 [2];
};

struct IpAddr : Net32 {
    constexpr IpAddr (uint32_t v =0) : Net32 (v) {}
    constexpr IpAddr (uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4)
        : Net32 ((v1<<24)|(v2<<16)|(v3<<8)|v4) {}

    auto asStr (SmallBuf& buf =smallBuf) const {
        auto p = (uint8_t const*) this;
        auto ptr = buf;
        for (int i = 0; i < 4; ++i)
            ptr += snprintf(ptr, 5, "%c%d", i == 0 ? ' ' : '.', p[i]);
        return buf + 1;
    }
};

struct Peer {
    IpAddr _rIp;
    uint16_t _rPort, _lPort;

    auto operator== (Peer const& p) const {
        return _rPort == p._rPort && _lPort == p._lPort && _rIp == p._rIp;
    }
};

struct Interface {
    MacAddr const _mac;
    IpAddr _ip, _gw, _dns, _sub;

    Interface (MacAddr const& mac) : _mac (mac) {}

    virtual auto canSend () -> Chunk =0;
    virtual auto send (void const* p, uint32_t n) -> int =0;
};

MacAddr const wildMac {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
IpAddr const wildIp {255,255,255,255};

template <int N>
struct ArpCache {
    void init (IpAddr myIp, MacAddr const& myMac) {
        memset(this, 0, sizeof *this);
        prefix = myIp & ~0xFF;
        add(myIp, myMac);
    }

    auto canStore (IpAddr ip) const { return (ip & ~0xFF) == prefix; }

    void add (IpAddr ip, MacAddr const& mac) {
        if (canStore(ip)) {
            // may run out of space, start afresh
            if (items[N-1].node != 0)
                memset(items+1, 0, (N-1) * sizeof items[0]);
            uint8_t b = ip;
            for (auto& e : items)
                if (e.node == 0 || b == e.node || mac == e.mac) {
                    e.node = b;
                    e.mac = mac;
                    return;
                }
        }
    }

    auto toIp (MacAddr const& mac) const -> IpAddr {
        for (auto& e : items)
            if (e.node != 0 && mac == e.mac)
                return prefix + e.node;
        return 0;
    }

    auto toMac (IpAddr ip) const -> MacAddr const& {
        if (canStore(ip))
            for (auto& e : items)
                if ((uint8_t) ip == e.node)
                    return e.mac;
        return wildMac;
    }

    void dump () const {
        debugf("ARP cache [%d]\n", N);
        for (auto& e : items)
            if (e.node != 0) {
                SmallBuf sb;
                IpAddr ip = prefix + e.node;
                debugf("  %s = %s\n", ip.asStr(), e.mac.asStr(sb));
            }
    }

private:
    struct {
        MacAddr mac;
        uint8_t node; // low byte of IP address
    } items [N];
    
    uint32_t prefix =0;
};

ArpCache<10> arpCache;

struct OptionIter {
    OptionIter (uint8_t* p, uint8_t o =0) : ptr (p), off (o) {}

    operator bool () const { return *ptr != 0xFF; }

    auto next () {
        auto typ = *ptr++;
        len = *ptr++;
        ptr += len;
        return typ;
    }

    void extract (void* p, uint32_t n =0) {
        ensure (n == 0 || n == len);
        memcpy(p, ptr-len, len);
    }

    void append (uint8_t typ, void const* p, uint8_t n) {
        *ptr++ = typ;
        *ptr++ = n + off;
        memcpy(ptr, p, n);
        ptr += n;
        *ptr = off ? 0x00 : 0xFF;
    }

    uint8_t* ptr;
    uint8_t len =0;
    uint8_t off;
};

struct Frame {
    MacAddr _dst, _src;
    Net16 _typ;

    Frame (Net16 typ) : _typ (typ) {}

    void isReply (Interface const& ni) {
        _dst = _src;
        _src = ni._mac; 
    }

    void received (Interface&); // dispatcher
};
static_assert(sizeof (Frame) == 14);

struct Arp : Frame {
    Net16 _hw =1, _proto =0x0800;
    uint8_t _macLen =6, _ipLen =4;
    Net16 _op =1;
    MacAddr _sendMac;
    IpAddr _sendIp;
    MacAddr _targMac;
    IpAddr _targIp;

    Arp () : Frame (0x0806) {}

    void isReply (Interface const& ni) {
        _dst = _src;
        _src = ni._mac; 
        _targMac = _sendMac;
        _targIp = _sendIp;
        _sendMac = _src;
        _sendIp = ni._ip;
    }

    void received (Interface& ni) {
        if (_op == 1 && _targIp == ni._ip) { // ARP request
SmallBuf sb;
debugf("ARP %s %s\n", _sendIp.asStr(), _src.asStr(sb));
            isReply(ni);
            _op = 2; // ARP reply
            ni.send(this, sizeof *this);
        }
    }
};
static_assert(sizeof (Arp) == 42);

struct Ip4 : Frame {
    uint8_t _versLen =0x45, _tos =0;
    Net16 _total, _id, _frag =0;
    uint8_t _ttl =64, _proto;
    Net16 _hcheck =0;
    IpAddr _srcIp, _dstIp;

    Ip4 (uint8_t proto) : Frame (0x0800), _proto (proto) {
        static uint16_t gid;
        _id = ++gid;
    }

    void isReply (Interface const& ni) {
        Frame::isReply(ni);
        _dstIp = _srcIp;
        _srcIp = ni._ip;
    }

    void received (Interface&); // dispatcher

    void sendIt (Interface& ni, uint16_t len) {
        _total = len - 14;
        ni.send(this, len);
    }
};
static_assert(sizeof (Ip4) == 34);

struct Icmp : Ip4 {
    uint8_t _type, _code;
    Net16 _sum;

    Icmp () : Ip4 (1) {}

    void received (Interface&) {
        debugf("ICMP type %d code %d\n", _type, _code);
        dumpHex(this, sizeof *this);
    }
};
static_assert(sizeof (Icmp) == 38);

struct Udp : Ip4 {
    Net16 _sPort, _dPort, _len, _sum;

    Udp () : Ip4 (17) {}

    void isReply (Interface const& ni) {
        Ip4::isReply(ni);
        swap(_sPort, _dPort);
    }

    void received (Interface&); // dispatcher

    void sendIt (Interface& ni, uint16_t len) {
        _len = len - 34;
        Ip4::sendIt(ni, len);
    }
};
static_assert(sizeof (Udp) == 42);

struct Dhcp : Udp {
    uint8_t _op =1, _htype =1, _hlen =6, _hops =0;
    Net32 _tid =0;
    Net16 _sec =0, _flags =(1<<15);
    IpAddr _clientIp, _yourIp, _serverIp, _gwIp;
    uint8_t _clientHw [16], _hostName [64], _fileName [128];
    uint8_t _options [312];

    enum { Subnet=1,Router=3,Dns=6,Domain=15,Bcast=28,Ntps=42,
            ReqIp=50,Lease=51,MsgType=53,ServerIp=54,ReqList=55 };

    Dhcp () {
        _dst = wildMac;
        _dstIp = wildIp;
        _dPort = 67;
        _sPort = 68;
        memset(_clientHw, 0, 16+64+128+312);
        static uint8_t const cookie [] {99, 130, 83, 99};
        memcpy(_options, cookie, sizeof cookie);
    }

    void discover (Interface& ni) {
        _src = ni._mac;
        memcpy(_clientHw, &ni._mac, sizeof ni._mac);

        OptionIter it {_options+4};
        it.append(MsgType, "\x01", 1);         // discover
        it.append(ReqList, "\x01\x03\x06", 3); // subnet router dns

        sendIt(ni, (uintptr_t) it.ptr + 1 - (uintptr_t) this);
        //sendIt(ni, sizeof *this);
        //sendIt(ni, 400);
    }

    auto request (Interface& ni) {
        isReply(ni);
        _op = 2; // DHCP reply
        _clientIp = _yourIp;
        memcpy(_clientHw, &ni._mac, sizeof ni._mac);

        OptionIter it {_options+4};
        it.append(MsgType, "\x03", 1); // request
        it.append(ServerIp, &_serverIp, 4);
        it.append(ReqIp, &_clientIp, 4);

        sendIt(ni, (uintptr_t) it.ptr + 1 - (uintptr_t) this);
    }

    void received (Interface& ni) {
        auto reply = _options[6];
        if (reply == 2 || reply == 5) { // Offer or ACK
            ni._ip = _yourIp;
            OptionIter it {_options+4};
            while (it)
                switch (auto typ = it.next(); typ) {
                    case Subnet: it.extract(&ni._sub, 4); break;
                    case Dns:    it.extract(&ni._dns, 4); break;
                    case Router: it.extract(&ni._gw,  4); break;
                    //default:     debugf("option %d\n", typ); break;
                }
            if (reply == 2) { // Offer
                arpCache.init(ni._ip, ni._mac);
                request(ni);
            } else { // ACK
                SmallBuf sb [3];
                debugf("DHCP %s gw %s sub %s dns %s\n",
                        ni._ip.asStr(), ni._gw.asStr(sb[0]),
                        ni._sub.asStr(sb[1]), ni._dns.asStr(sb[2]));
                arpCache.add(_srcIp, _src); // DHCP server, i.e. gateway
                arpCache.dump();
            }
        }
    }
};
static_assert(sizeof (Dhcp) == 590);

template <int N>
struct Listeners {
    void add (uint16_t port) {
        for (auto& e : ports)
            if (e == 0) {
                e = port;
                return;
            }
        ensure(0); // ran out of slots
    }
    void remove (uint16_t port) {
        for (auto& e : ports)
            if (e == port)
                e = 0;
    }
    auto exists (uint16_t port) const {
        for (auto e : ports)
            if (e == port)
                return true;
        return false;
    }
private:
    uint16_t ports [N];
};

Listeners<10> listeners;

#define debugf(...)
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
        uint32_t lUna, rUna;
        uint16_t pend;
        uint8_t state;
        ByteVec iBuf, oBuf;

        void init (Peer const& p, uint8_t s) {
            *(Peer*) this = p;
            pend = 0;
            state = s;
            iBuf.adj(1000); oBuf.adj(1000); // set capacities
            oBuf.insert(0, 6); memcpy(oBuf.begin(), "hello\n", 6); // test data
        }

        void deinit () {
            state = FREE;
            iBuf.clear();
            oBuf.clear();
        }

        void dump () const {
            debugf("  %c %34s rUna %04x lUna %04x  iBuf %d oBuf %d @ %d ms\n",
                    "FLRSE12GTCK"[state], "",
                    (uint16_t) rUna, (uint16_t) lUna,
                    iBuf.size(), oBuf.size(), millis());
        }
    };

    void sendReply (Interface& ni, Session& ts, uint8_t flags, uint16_t bytes) {
        auto win = ts.iBuf.cap() - ts.iBuf.size();
        debugf("  > %s %28s ack %04x  seq %04x   len %d win %d\n",
                decode(flags, "FSRPAU"), "",
                (uint16_t) ts.rUna, (uint16_t) ts.lUna, bytes, win);
        Ip4::isReply(ni);
        swap(_sPort, _dPort);
        _seq = ts.lUna;
        _ack = flags & ACK ? ts.rUna : 0;
        _off = 5<<4;
        _code = flags;
        _win = win;
        sendIt(ni, sizeof *this + bytes);
    }

    struct Reply { uint8_t state, flags; uint16_t bytes; };

    auto process (Session& ts) -> Reply {
        uint8_t state = ts.state;
        uint8_t flags = 0;

        if (state == LSTN && (_code & SYN)) {
ts.oBuf.clear();
            ts.lUna = 1024; // TODO 1024 -> random
            ts.rUna = _seq+1;
            return {SYNR, SYN+ACK, 0};
        }

        if (state == SYNR && _ack >= ts.lUna + 1) {
            ++ts.lUna;
            state = ESTB;
            return {state, 0, 0};
        }

        if (state == ESTB || state == FIN1 || state == FIN2) {
            uint16_t rAdv = _seq - ts.rUna;
            ts.rUna += rAdv;
        }

        auto nIn = _total - 4*(_off>>4) - 20;
        auto nOut = 0;

        if (state == ESTB || state == CLOW) {
            uint16_t lAdv = _ack - ts.lUna;
            ts.oBuf.remove(0, lAdv);
            ts.lUna += lAdv;
            ts.pend -= lAdv;

            dumpHex(_data, nIn);
            ts.iBuf.insert(ts.iBuf.size(), nIn);
            memcpy(ts.iBuf.end() - nIn, _data, nIn);
            ts.rUna += nIn;

            nOut = ts.oBuf.size();
            if (nOut > _win)
                nOut = _win;
            if (nOut > 0) {
                memcpy(_data, ts.oBuf.begin(), nOut);
                ts.pend += nOut;
                flags |= ACK;
            }
        }

        switch (state) {
            case ESTB: {
                if (_code & FIN) {
                    ++ts.rUna;
                    flags |= ACK;
                    state = CLOW;
                } else if (ts.oBuf.size() == 0) {
                    flags |= FIN;
                    state = FIN1;
                }
                break;
            }
            case FIN1:
                if (_code & FIN) {
                    ++ts.rUna;
                    flags |= ACK;
                    state = CSNG;
                }
                if (_code & ACK) {
                    ++ts.lUna;
                    state = _code & FIN ? TIMW : FIN2;
                }
                break;
            case FIN2:
                if (_code & FIN) {
                    ++ts.rUna;
                    flags |= ACK;
                    state = TIMW;
                }
                break;
            case CSNG:
                if (_code & ACK) {
                    ++ts.lUna;
                    state = TIMW;
                }
                break;
            case TIMW:
                break; // TODO timer
            case CLOW: {
                if (_code & ACK)
                    ++ts.lUna;
                if (ts.oBuf.size() == 0) {
                    flags |= FIN;
                    state = LACK;
                }
                break;
            }
            case LACK:
                if (_code & ACK)
                    ts.deinit();
                break; // TODO timer
        }

        return {state, flags, nOut};
    }

    void received (Interface& ni) {
        SmallBuf sb;
        debugf("TCP %s %s:%d -> :%d  seq %04x  ack %04x total %d @ %d ms\n",
                decode(_code, "FSRPAU"), _srcIp.asStr(sb),
                (int) _sPort, (int) _dPort, (uint16_t) _seq, (uint16_t) _ack,
                (int) _total, millis());

        auto sess = findSession({_srcIp, _sPort, _dPort});
        if (sess != nullptr) {
            sess->dump();
            auto [state, flags, bytes] = process(*sess);
            sess->state = state;
            if (flags != 0)
                sendReply(ni, *sess, flags, bytes);
            if (sess->state == TIMW)
                sess->deinit(); // TODO no timer yet
            sess->dump();
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

Tcp::Session Tcp::sessions [];

void Udp::received (Interface& ni) {
    switch (_dPort) {
        case 68: ((Dhcp*) this)->received(ni); break;
        //default: dumpHex((uint8_t const*) this + sizeof (Udp), _len-8);
    }
}

void Ip4::received (Interface& ni) {
    switch (_proto) {
        case 1:  ((Icmp*) this)->received(ni); break;
        case 6:  ((Tcp*) this)->received(ni); break;
        case 17: ((Udp*) this)->received(ni); break;
        default: debugf("proto %02x\n", (int) _proto); break;
    }
}

void Frame::received (Interface& ni) {
    switch (_typ) {
        case 0x0800: ((Ip4*) this)->received(ni); break;
        case 0x0806: ((Arp*) this)->received(ni); break;
        //default:     debugf("frame %04x\n", (int) _typ); break;
    }
}
#undef debugf
