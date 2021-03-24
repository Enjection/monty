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

struct UartInfo {
    uint8_t num; IrqVec irq; uint32_t base;
} const uartInfo [] = {
    %s
};

struct SpiInfo {
    uint8_t num; IrqVec irq; uint32_t base;
} const spiInfo [] = {
    %s
};
""".strip()

# replace each series of digits with a three-digit number
def numsort(s):
    return re.sub('\d+', (lambda m: ("000" + m.group(0))[-3:]), s)

def uartsort(s):
    if s[:5] == "USART": # group UART and USART together
        s = "UART" + s[5:]
    return numsort(s)

irqs = {}
groups = {}

periphs = []
for p in sorted(parser.get_device().peripherals, key=lambda p: numsort(p.name)):
    u = p.name.upper()
    g = p.group_name.upper()
    b = p.base_address

    if not g in groups:
        groups[g] = []
    groups[g].append(u)

    periphs.append("constexpr auto %-13s = 0x%08x;  // %s" % (u, b, g))

    for x in p.interrupts:
        irqs[x.name] = (x.value, g)

irqvecs = []
for k in sorted(irqs.keys(), key=uartsort):
    v, g = irqs[k]
    irqvecs.append(f"{k:<21} = {v:3},  // {g}")

uarts = []
for p in sorted(groups["USART"], key=uartsort):
    if p[0] == "U": # ignore LPUART
        n = int(re.sub('\D+', '', p))
        uarts.append("{ %2d, IrqVec::%-6s, %-6s }," % (n, p, p))

spis = []
for p in sorted(groups["SPI"], key=uartsort):
    if p[0] == "S": # ignore I2S
        n = int(re.sub('\D+', '', p))
        spis.append("{ %d, IrqVec::%s, %s }," % (n, p, p))

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
