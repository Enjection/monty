template <typename T>
void swap (T& a, T& b) { T t = a; a = b; b = t; }

struct MacAddr {
    uint8_t b [6];

    auto asStr (SmallBuf& buf =smallBuf) const {
        auto ptr = buf;
        for (int i = 0; i < 6; ++i)
            ptr += snprintf(ptr, 4, "%c%02x", i == 0 ? ' ' : ':', b[i]);
        return buf + 1;
    }
};

struct Net16 {
    constexpr Net16 (uint16_t v =0) : b1 {(uint8_t) (v>>8), (uint8_t) v} {}
    operator uint16_t () const { return (b1[0]<<8) | b1[1]; }
private:
    uint8_t b1 [2];
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

struct Interface {
    MacAddr const _mac;
    IpAddr _ip, _gw, _dns, _sub;

    Interface (MacAddr const& mac) : _mac (mac) {}

    virtual auto canSend () -> Chunk =0;
    virtual void send (void const* p, uint32_t n) =0;
};

MacAddr const wildMac {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
IpAddr const wildIp {0xFFFFFFFF};

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
                if (e.node == 0 || b == e.node ||
                        memcmp(&mac, &e.mac, sizeof mac) == 0) {
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
    OptionIter (uint8_t* p) : ptr (p) {}

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
        *ptr++ = n;
        memcpy(ptr, p, n);
        ptr += n;
        *ptr = 0xFF;
    }

    uint8_t* ptr;
    uint8_t len =0;
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
    uint8_t _ttl =255, _proto;
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
        _total = len - 20;
        ni.send(this, len);
    }
};
static_assert(sizeof (Ip4) == 34);

struct Udp : Ip4 {
    Net16 _sPort, _dPort, _len, _sum;

    Udp () : Ip4 (17) {}

    void isReply (Interface const& ni) {
        Ip4::isReply(ni);
        swap(_sPort, _dPort);
    }

    void received (Interface&); // dispatcher

    void sendIt (Interface& ni, uint16_t len) {
        _len = len - 40;
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

        //sendIt(ni, (uint32_t) it.ptr + 9 - (uint32_t) this);
        //sendIt(ni, sizeof *this);
        sendIt(ni, 400);
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

        sendIt(ni, (uint32_t) it.ptr + 9 - (uint32_t) this);
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

struct Tcp : Ip4 {
    Net16 _sPort, _dPort;
    Net32 _seq, _ack;
    Net16 _code, _window, _sum, _urg;
    uint8_t data [];

    Tcp () : Ip4 (6) {}

    void received (Interface&) {
        SmallBuf sb;
        debugf("TCP %s:%d -> %s:%d win %d seq %08x ack %08x\n",
                _srcIp.asStr(), (int) _sPort,
                _dstIp.asStr(sb), (int) _dPort,
                (int) _window, (int) _seq, (int) _ack);
    }
};
static_assert(sizeof (Tcp) == 54);

void Udp::received (Interface& ni) {
    switch (_dPort) {
        case 68: ((Dhcp*) this)->received(ni); break;
        default: dumpHex((uint8_t const*) this + sizeof (Udp), _len-8);
    }
}

void Ip4::received (Interface& ni) {
    switch (_proto) {
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
