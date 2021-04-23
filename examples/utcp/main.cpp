#include <monty.h>
#include <arch.h>
#include <cassert>
#include <cstdio>
#include <unistd.h>

using namespace monty;

#define ensure assert

using SmallBuf = char [20];
SmallBuf smallBuf;

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

auto millis () -> uint32_t {
    return nowAsTicks();
}

struct Stream {
    int fd;

    auto write (uint8_t const* p, uint32_t n) -> int {
printf("WRITE %d\n", n);
dumpHex(p, n);
        return ::write(fd, p, n);
    }
};

struct Device {
};

#include "../f750/net.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <fcntl.h>
#include <arpa/inet.h>

constexpr auto PORT = 8888; // filter TCP packets to or from this port

auto initBpf () -> int {
    char dev [20];
    int fd = -1;
    for (int i = 0; fd < 0 && i < 100; i++) {
        snprintf(dev, sizeof dev, "/dev/bpf%u", i);
        fd = open(dev, O_RDWR);
    }
    assert(fd >= 0);

    u_int32_t enable = 1, disable = 0;
    struct ifreq ifr;
    strcpy(ifr.ifr_name, "en0");

    int e;
    e = ioctl(fd, BIOCSETIF, &ifr);        assert(e >= 0);
    e = ioctl(fd, BIOCSHDRCMPLT, &enable); assert(e >= 0);
    e = ioctl(fd, BIOCSSEESENT, &disable); assert(e >= 0);
    e = ioctl(fd, BIOCIMMEDIATE, &enable); assert(e >= 0);

    static struct bpf_insn insns [] = {
        BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),         // check 16b @12
        BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ETHERTYPE_IP, 0, 10),
        BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 23),         // check 8b @23
        BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, IPPROTO_TCP, 0, 8),
        BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 20),         // check unfragmented
        BPF_JUMP(BPF_JMP+BPF_JSET+BPF_K, 0x1FFF, 6, 0),
        BPF_STMT(BPF_LDX+BPF_B+BPF_MSH, 14),        // get header length
        BPF_STMT(BPF_LD+BPF_H+BPF_IND, 14),         // ... either srcIp matches
        BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, PORT, 2, 0),
        BPF_STMT(BPF_LD+BPF_H+BPF_IND, 16),         // .. or dstIp matches
        BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, PORT, 0, 1),
        BPF_STMT(BPF_RET+BPF_K, ~0U),               // accept
        BPF_STMT(BPF_RET+BPF_K, 0),                 // reject
    };

    static struct bpf_program fcode;
    fcode.bf_len = sizeof insns / sizeof *insns;
    fcode.bf_insns = insns;
    e = ioctl(fd, BIOCSETF, &fcode); assert(e >= 0);

    printf("%s :%d fd %d\n", dev, PORT, fd);
    return fd;
}

int main () {
    arch::init(10*1024);

    Interface bpf ({0xF0,0x18,0x98,0xF2,0x6C,0xE4});
    bpf.fd = initBpf();

    listeners.add(PORT);

    size_t bpfMax;
    auto e = ioctl(bpf.fd, BIOCGBLEN, &bpfMax); assert(e >= 0);
    uint8_t bpfBuf alignas(4) [bpfMax];

    bpf._ip = {192,168,188,10};

    while (true) {
        auto n = read(bpf.fd, bpfBuf, sizeof bpfBuf);
        if (n <= 0)
            break;
        auto p = bpfBuf;
        while (p < bpfBuf + n) {
            auto bh = (struct bpf_hdr*) p;
            auto len = bh->bh_hdrlen;

            dumpHex(p + len, 64);
            ((Frame*) (p + len))->received(bpf);

            p += BPF_WORDALIGN(len + bh->bh_caplen);
        }
    }

    arch::done();
}
