#!/usr/bin/env python3
#
# Extract STM32 chip information from the *.svd files in PlatformIO.
# The generated code includes "//CG" directives for Monty's code generator.
# This is usually called by "inv env" / "inv x-env" when switching PIO env's.
#
# usage: src/device.py svdname ?~/.platformio/platforms/ststm32/misc/svd?

import configparser, os, re, sys
from os import path
from cmsis_svd.parser import SVDParser

myDir = path.dirname(sys.argv[0])
svdName = sys.argv[1]

if len(sys.argv) > 2:
    svdDir = sys.argv[2]
else:
    svdDir = path.expanduser("~/.platformio/platforms/ststm32/misc/svd")

# need some extra info, since the SVD files are incomplete (and inconsistent)
cfg = configparser.ConfigParser(interpolation=configparser.ExtendedInterpolation())
cfg.read(path.join(myDir, "stm32.ini"))
patches = cfg[svdName[:7]]

parser = SVDParser.for_xml_file("%s/%s.svd" % (svdDir, svdName))

template = """
// from: %s.svd

//CG< periph
%s
//CG>

// useful registers
%s

enum struct IrqVec : uint8_t {
    //CG< irqvec
    %s
    //CG>
    limit = %d
};

struct DmaInfo { uint32_t base; uint8_t streams [8]; };

struct DevInfo {
    uint8_t pos :4, num :4, ena;
    uint8_t rxDma :1, rxChan :4, rxStream :3;
    uint8_t txDma :1, txChan :4, txStream :3;
    IrqVec irq;
    uint32_t base;
};

template< uint32_t N >
constexpr auto findDev (DevInfo const (&map) [N], int num) -> DevInfo const& {
    for (auto& e : map)
        if (num == e.num)
            return e;
    return map[0]; // verify num to check that it was found
}

DmaInfo const dmaInfo [] = {
    %s
};

DevInfo const uartInfo [] = {
    %s
};

DevInfo const spiInfo [] = {
    %s
};
""".strip()

# get patch info from the str32.ini file, if present
def patch(name):
    return patches.get(name, "").split()

# there are inconsistenties in the SVD files, use UART iso USART everywhere
def uartfix(s):
    if s.startswith("USART"):
        s = "UART" + s[5:]
    return s

# replace each series of digits with a three-digit number
def numsort(s):
    return re.sub('\d+', (lambda m: ("000" + m.group(0))[-3:]), s)

def uartsort(s):
    return numsort(uartfix(s))

irqs = {}
irqLimit = 0
groups = {}

periphs = []
for p in sorted(parser.get_device().peripherals, key=lambda p: uartsort(p.name)):
    u = p.name.upper()
    g = p.group_name.upper()
    b = p.base_address

    if not g in groups:
        groups[g] = []
    groups[g].append(u)

    periphs.append("constexpr auto %-13s = 0x%08X;  // %s" % (uartfix(u), b, g))

    for x in p.interrupts:
        irqs[x.name] = (x.value, g)
        if irqLimit <= x.value:
            irqLimit = x.value + 1

    if 0: # can be used to print out specific details
        for r in p.registers:
            off = "0x%02X" % r.address_offset
            if g == "RCC":
                #print(u, r.name, off)
                if r.name[4:7] == "ENR":
                    for f in r.fields:
                        #print(" ", f.name)
                        if uartfix(f.name).startswith("SPI"):
                            print(" ", r.name, off, f.name, f.bit_offset)

# additional IRQs, missing or wrong in SVD
for x in patch("irqs"):
    n, i, g = x.split(",")
    if n in groups[g]:
        irqs[n] = (int(i), g)

regs = []
for r in ["rcc"]:
# see https://stackoverflow.com/questions/1624883/ ... pfff
    for k, v in zip(*(iter(patch("rcc")),) * 2):
        u = r.upper()
        regs.append("constexpr auto %s_%s = %s + %s;" % (u, k.upper(), u, v))

irqvecs = []
for k in sorted(irqs.keys(), key=uartsort):
    v, g = irqs[k]
    u = uartfix(k)
    irqvecs.append(f"{u:<21} = {v:3},  // {g}")

dmas = []
for k, v in zip(*(iter(patch("dma")),) * 2):
    streams = 8 * ["0"]
    for q in irqs:
        if q.startswith(k) and v in q:
            s = re.search(r"(\d)_(\d)$", q)
            if s: # deal with "DMA1_Channel4_7"
                first, last = s.group(1, 2)
                for i in range(int(first), int(last)+1):
                    streams[int(i)] = str(irqs[q][0])
            else:
                streams[int(q[-1])] = str(irqs[q][0])
    dmas.append("{ %s, { %s }}," % (k, ", ".join(streams)))

uarts = []
for p in sorted(groups["USART"], key=uartsort):
    enaBits = patch("ena_uart")
    drxBits = patch("dma_uart_rx")
    dtxBits = patch("dma_uart_tx")
    if p[0] == "U": # ignore LPUART
        n = int(re.sub('\D+', '', p))
        u = uartfix(p)
        e = enaBits[n-1] if n-1 < len(enaBits) else "0"
        r = drxBits[n-1] if n-1 < len(drxBits) else "1c0s0"
        t = dtxBits[n-1] if n-1 < len(dtxBits) else "1c0s0"
        rxd, rxc, rxs = r[:1], r[2:-2], r[-1:]
        txd, txc, txs = t[:1], t[2:-2], t[-1:]
        if t == "-": 
            txd, txc, txs = "2", "15", "0" # unlikely value ...
        uarts.append("{ %d,%2d, %2s, %s,%2s, %s, %s,%2s, %s, IrqVec::%-6s, %-6s}," % \
            (len(uarts), n, e, int(rxd)-1, rxc, rxs, int(txd)-1, txc, txs, u, u))

spis = []
for p in sorted(groups["SPI"], key=uartsort):
    enaBits = patch("ena_spi")
    if p[0] == "S": # ignore I2S
        n = int(re.sub('\D+', '', p))
        if p in irqs:
            e = enaBits[n-1] if n-1 < len(enaBits) else "0"
            spis.append("{ %d, %d, %s, 0, 0, 0, 0, 0, 0, IrqVec::%s, %s }," % \
                        (len(spis), n, e, p, p))
        else:
            print("no IrqVec for %s, ignoring" % p, file=sys.stderr)

if 0: # don't do register offsets (yet), the data is not consistent enough
    print()
    for p in sortedp:
        d = device['peripherals'][p]
        if 'derived_from' not in d:
            g = d['group_name'].upper()
            print("    enum %s {" % g)
            display_registers(g + "_", device['peripherals'][p], level=1)
            print("    };")

if 1:
    print(template % (svdName,
                      "\n".join(periphs),
                      "\n".join(regs),
                      "\n    ".join(irqvecs),
                      irqLimit,
                      "\n    ".join(dmas),
                      "\n    ".join(uarts),
                      "\n    ".join(spis)))
