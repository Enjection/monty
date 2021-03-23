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
                            f = os.path.basename(fname).split("_")[0] + ":"
                            print(f, family, rxtx, gpio, uart, "alt-mode:",
                                  mode, "<->", m[uart], "?", file=sys.stderr)
                        m[uart] = mode

# replace each series of digits with a three-digit number
def numsort(s):
    return re.sub('\d+', (lambda m: ("000" + m.group(0))[-3:]), s)

ipDir = sys.argv[1]

print("""
namespace altpins {
    constexpr auto Pin (char const* s) {
        int r = *s++ - 'A', n = 0;
        while (*s)
            n = 10 * n + *s++ - '0';
        return (r << 4) | n;
    }

    struct AltPins {
        uint16_t pin :8;
        uint16_t dev :4;
        uint16_t alt :4;
    };
""".strip())

for family in "F0 F2 F3 F4 F7 G0 G4 H7 L0 L1 L4 L5 WB WL".split():
#for family in "L4".split():
    db = { "RX": {}, "TX": {} }
    for fn in os.listdir(ipDir):
        if "STM32" + family in fn:
            #print(fn)
            try:
                extract(os.path.join(ipDir, fn))
            except:
                print(fn,gpio,signal,alt)
                raise
    print("\n#if STM32%s" % family)
    for rxtx in sorted(db):
        print("    AltPins const uartAlt%s [] = {" % rxtx)
        for gpio in sorted(db[rxtx], key=numsort):
            for uart in sorted(db[rxtx][gpio]):
                mode = db[rxtx][gpio][uart]
                print("        { Pin(%-5s),%2s,%2s }," % ('"%s"' % gpio, uart, mode))
        print("    };")
    print("#endif")

print("}")
