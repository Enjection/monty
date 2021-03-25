#!/usr/bin/env python3
#
# Extract STM32 chip information from the *.svd files in PlatformIO.
# The generated code includes "//CG" directives for Monty's code generator.
# This is usually called by "inv env" / "inv x-env" when switching PIO env's.
#
# usage: src/device.py svdname ?~/.platformio/platforms/ststm32/misc/svd?

import os, re, sys
from os import path
from cmsis_svd.parser import SVDParser

svdName = sys.argv[1]

if len(sys.argv) > 2:
    svdDir = sys.argv[2]
else:
    svdDir = path.expanduser("~/.platformio/platforms/ststm32/misc/svd")

parser = SVDParser.for_xml_file("%s/%s.svd" % (svdDir, svdName))

template = """
// from: %s.svd

//CG< periph
%s
//CG>

enum struct IrqVec : uint8_t {
    //CG< irqvec
    %s
    //CG>
};

struct DevInfo { uint8_t num; IrqVec irq; uint32_t base; };

template< size_t N >
constexpr auto findDev (DevInfo const (&map) [N], int num) -> DevInfo const& {
    for (auto& e : map)
        if (num == e.num)
            return e;
    return map[0]; // verify num to check that it was found
}

DevInfo const uartInfo [] = {
    %s
};

DevInfo const spiInfo [] = {
    %s
};
""".strip()

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
groups = {}

periphs = []
for p in sorted(parser.get_device().peripherals, key=lambda p: uartsort(p.name)):
    u = p.name.upper()
    g = p.group_name.upper()
    b = p.base_address

    if not g in groups:
        groups[g] = []
    groups[g].append(u)

    periphs.append("constexpr auto %-13s = 0x%08x;  // %s" % (uartfix(u), b, g))

    for x in p.interrupts:
        irqs[x.name] = (x.value, g)

irqvecs = []
for k in sorted(irqs.keys(), key=uartsort):
    v, g = irqs[k]
    u = uartfix(k)
    irqvecs.append(f"{u:<21} = {v:3},  // {g}")

uarts = []
for p in sorted(groups["USART"], key=uartsort):
    if p[0] == "U": # ignore LPUART
        n = int(re.sub('\D+', '', p))
        u = uartfix(p)
        uarts.append("{ %2d, IrqVec::%-6s, %-6s }," % (n, u, u))

spis = []
for p in sorted(groups["SPI"], key=uartsort):
    if p[0] == "S": # ignore I2S
        n = int(re.sub('\D+', '', p))
        if p in irqs:
            spis.append("{ %d, IrqVec::%s, %s }," % (n, p, p))
        else:
            print("no IrqVec for %s, ignoring" % p, file=sys.stderr)

if False: # don't do register offsets (yet), the data is not consistent enough
    print()
    for p in sortedp:
        d = device['peripherals'][p]
        if 'derived_from' not in d:
            g = d['group_name'].upper()
            print("    enum %s {" % g)
            display_registers(g + "_", device['peripherals'][p], level=1)
            print("    };")

print(template % (svdName,
                  "\n".join(periphs),
                  "\n    ".join(irqvecs),
                  "\n    ".join(uarts),
                  "\n    ".join(spis)))
