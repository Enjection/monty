#!/usr/bin/env python3
#
# usage: src/altpins.py mcu-ip-dir

import os, re, sys

# replace each series of digits with a three-digit number
def numsort(s):
    return re.sub('\d+', (lambda m: ("000" + m.group(0))[-3:]), s)

# use regular expressions to extract info from a (well-formatted) XML file ...
def extract(fname):
    gpio, signal, spec, mode = "", "", "", ""

    def addToDb(dev, pin):
        if pin in db:
            if gpio not in db[pin]:
                db[pin][gpio] = {}
            m = db[pin][gpio]
            if dev in m and mode != m[dev]:
                f = os.path.basename(fname) + ":"
                # conflicting alt mode for same dev/pin combo !
                print(f, family, pin, gpio, dev, "alt-mode:",
                        m[dev], "<->", mode, "?", file=sys.stderr)
            m[dev] = mode

    def processPin():
        if signal[:1] == "U" and "ART" in signal: # not UCPD
            u = signal
            if u.startswith("UART"):
                u = "USART" + u[4:]
            dev, pin = u.split("_")[:2]
            addToDb(dev[5:], pin)
        if signal[:3] == "SPI":
            dev, pin = signal.split("_")
            addToDb(dev[3:], pin)

    with open(fname) as f:
        # pick up GPIO_Pin, PinSignal, Specific, and Possible tags
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
                # yuck, but it gets the job done, i.e. AF<mode> ...
                mode = re.findall(r'>.+<', s)[0][6:-1].split("_")[0][2:]
                # context is now complete, deal with this pin
                processPin()

# build database for current family
def scan():
    for fn in os.listdir(ipDir):
        if "STM32" + family in fn:
            try:
                extract(os.path.join(ipDir, fn))
            except:
                print(fn,gpio,signal,mode) # FIXME vars not available here
                raise

# generate code for current family
def emit():
    print("\n#if STM32%s" % family)
    for pin in db:
        print("    AltPins const alt%s [] = {" % pin)
        for gpio in sorted(db[pin], key=numsort):
            for dev in sorted(db[pin][gpio]):
                mode = db[pin][gpio][dev]
                print("        { Pin(%-5s),%2s,%2s }," % \
                    ('"%s"' % gpio, dev, mode))
        print("    };")
    print("#endif")

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
    db = { x:{} for x in "RX TX MISO MOSI SCK NSS".split() }
    scan()
    emit()

print("}")
