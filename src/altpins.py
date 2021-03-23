#!/usr/bin/env python3
#
# usage: src/altpins.py mcu-ip-dir

import os, re, sys

def extract(fname):
    gpio, signal, spec, alt = "", "", "", ""
    with open(fname) as f:
        for line in f:
            s = line.strip()
            if s.startswith("<GPIO_Pin "):
                m = re.findall(r'"P\D\d+', s)
                if not m: # e.g. "PDR" in F410
                    continue
                gpio = m[0][2:] # e.g. A10
            elif s.startswith("<PinSignal "):
                signal = re.findall(r'Name=".+"', s)[0][6:-1]
            elif s.startswith("<Specific"):
                spec = re.findall(r'"GPIO_AF"', s)
            elif spec and s.startswith("<Possible"):
                alt = re.findall(r'>.+<', s)
                if signal[:1] == "U" and "ART" in signal: # not UCPD
                    if "RX" in signal or "TX" in signal:
                        mode = alt[0][6:-1]
                        if mode[:2] == "AF":
                            mode = mode.split("_")[0][2:]
                        else:
                            mode = ""
                        u = signal
                        if u.startswith("UART"):
                            u = "USART" + u[4:]
                        uart, rxtx = u.split("_")
                        uart = uart[5:]
                        #print(signal, gpio, alt)
                        #print("%s %-4s %-2s %s" % (rxtx, gpio, uart, mode))
                        if gpio not in db[rxtx]:
                            db[rxtx][gpio] = {}
                        m = db[rxtx][gpio] 
                        if uart in m and mode != m[uart]:
                            f = fname.split("_")[0][8:]
                            print(family, f, rxtx, gpio, uart, ":", mode,
                                  "<->", m[uart], "?", file=sys.stderr)
                        m[uart] = mode

# replace each series of digits with a three-digit number
def widenum(m):
    return ("000" + m.group(0))[-3:]

def irqsort(s):
    if s[:5] == "USART": # group UART and USART together
        s = "UART" + s[5:]
    return re.sub('\d+', widenum, s)

dir = sys.argv[1]

print("""
namespace altpins {
    constexpr auto Pin (char const* s) {
        int r = *s++ - 'A', n = 0;
        while (*s)
            n = 10 * n + *s++ - '0';
        return (r << 4) | n;
    }

    struct UartAltPins {
        uint16_t pin  :8;
        uint16_t uart :4;
        uint16_t alt  :4;
    };
""".strip())

for family in "F0 F2 F3 F4 F7 G0 G4 H7 L0 L1 L4 L5 WB WL".split():
#for family in "L4".split():
    db = { "RX": {}, "TX": {} }
    for fn in os.listdir("IP"):
        if "STM32" + family in fn:
            #print(fn)
            try:
                extract(os.path.join("IP", fn))
            except:
                print(fn,gpio,signal,alt)
                raise
    print("\n#if STM32%s" % family)
    for rxtx in sorted(db):
        print("    UartAltPins const uartAlt%s [] = {" % rxtx)
        for gpio in sorted(db[rxtx], key=irqsort):
            for uart in sorted(db[rxtx][gpio]):
                mode = db[rxtx][gpio][uart]
                print("        { Pin(%-5s),%2s,%2s }," % ('"%s"' % gpio, uart, mode))
        print("    };")
    print("#endif")

print("}")
