#!/usr/bin/env python3

# Monty test runner
#
# Usage: runner.py [-v] [-i ignore,...] file...
#
#   files with a .py extension are first compiled to .mpy using mpy-cross,
#   but only if the .mpy does not exist or is older than the .py source

import os, re, subprocess, sys, time
# moved: import serial, serial.tools.list_ports

def findSerialPorts():
    """map[serial#] = (prod, dev)"""
    import serial.tools.list_ports
    found = []
    for p in serial.tools.list_ports.comports():
        if p.product:
            prod = p.product.split(',')[0]
            if prod.startswith('Black Magic Probe') and p.device[-1] == '1':
                continue # ignore BMP's upload port
            found.append((prod, p.device, p.serial_number))
    return found

def openSerialPort():
    import serial
    port = None
    serials = findSerialPorts()
    for prod, dev, serid in serials:
        print(f"{prod}: {dev} ser# {serid}")
        port = serial.Serial(dev, 921600, timeout=0.1)
    assert len(serials) == 1, f"{len(serials)} serial ports found"
    port.readlines() # clear out any old pending input
    return port

# convert bytes to intel hex, lines are generated per 32 bytes, e.g.
#   :200000004D05021F2054100A00071068656C6C6F2E70790011007B100A68656C6C6F100AC9
#   :0C002000776F726C643402595163000069
#   :00000001FF

def genHex(data, off=None):
    if off is not None:
        csum = -(2 + 2 + (off>>12) + (off>>4)) & 0xFF
        yield f":02000002{off>>4:04X}{csum:02X}\n"
    for i in range(0, len(data), 32):
        chunk = data[i:i+32]
        csum = -(len(chunk) + (i>>8) + i + sum(chunk)) & 0xFF
        line = f":{len(chunk):02X}{i:04X}00{chunk.hex().upper()}{csum:02X}\n"
        for i in range(0, len(line), 40):
            yield line[i:i+40] # send in pieces < 64 chars (for MacOS ???)
    yield ":00000001FF\n"

def compileIfOutdated(fn):
    root, ext = os.path.splitext(fn)
    if ext != '.py':
        return fn
    mpy = root + ".mpy"
    mtime = os.stat(fn).st_mtime
    if not os.path.isfile(mpy) or (mtime >= os.stat(mpy).st_mtime):
        # make any output from mpy-cross stand out in red
        base = os.path.basename(fn)
        out = subprocess.getoutput("mpy-cross -s %s %s" % (base, fn))
        if out:
            raise Exception(out)
    return mpy

def compileAndSend(ser, fn):
    try:
        mpy = compileIfOutdated(fn)
        # set watchdog and make sure it was accepted
        ser.reset_input_buffer()
        ser.write(b'wd 250\n')
        line = ser.readline().decode()
        if not line.endswith(" ms\n"):
            return line + "NO RESPONSE"
    except Exception as e:
        return e
    # send the bytecode as intel hex
    with open(mpy, "rb") as f:
        for line in genHex(f.read()):
            ser.write(line.encode())
            ser.flush()
            time.sleep(0.005) # needed for Black Magic Probe

def compareWithExpected (fn, output):
    adjout = output

    root = os.path.splitext(fn)[0]
    exp = root + ".exp"
    out = root + ".out"

    if os.path.isfile(exp):
        with open(exp) as f:
            expected = f.read()

        # process leading "/patt/repl/" matchers before comparing the output
        while expected[:1] == '/':
            head, expected = expected.split("\n", 1)
            patt, repl = head.split("/")[1:3]
            adjout = re.sub(re.compile(patt), repl, adjout)
        if adjout == expected:
            try:
                os.remove(out)
            except FileNotFoundError:
                pass
            return True

    with open(out, "w") as f:
        f.write(output)

    printSeparator(fn)
    if output and os.path.isfile(exp) and expected:
        if len(output.split("\n")) > 5 or len(expected.split("\n")) < 5:
            subprocess.run(f"diff - {exp} | head", shell=True,
                           input=adjout.encode())
        else:
            print(output, end="")

def printSeparator(fn, e=None):
    root = os.path.splitext(fn)[0]
    if e:
        msg = "FAIL"
    else:
        msg = ""
        try:
            with open(root + ".exp") as f:
                exp = f.readlines()
            nexp = len(exp)
        except:
            nexp = None
        try:
            with open(root + ".out") as f:
                out = f.readlines()
            nout = len(out)
            if nexp is None:
                msg = f"out: {nout}"
            elif nout == nexp:
                msg = f"out & exp: {nexp}"
            else:
                msg = f"out/exp: {nout}/{nexp}"
            if not nexp:
                e = ''.join(out[:10])[:-1]
        except:
            if nexp:
                msg = f"exp: {nexp}"
    sep = (50 - len(fn) - len(msg)) * "-"
    print("---------------------------", fn, sep, msg)
    if e:
        print(e)

if __name__ == "__main__":
    ser = openSerialPort()
    verbose, tests, match, fail, skip, ignores = 0, 0, 0, 0, 0, []

    args = iter(sys.argv[1:])
    for fn in args:
        if fn == "-v":
            verbose += 1
            continue
        if fn == "-i":
            iargs = next(args).split(",")
            ignores += iargs
            continue
        bn = os.path.basename(fn)
        if os.path.splitext(bn)[0] in ignores:
            continue
        if bn[1:2] == "_" and bn[0] != 's':
            skip += 1
            continue # skip non-stm32 tests
        tests += 1

        e = compileAndSend(ser, fn)
        if e:
            printSeparator(fn, e)
            fail += 1
            if e is Exception:
                continue
            else:
                break

        results = []
        ok = False
        delay = 2.5

        while True:
            try:
                line = ser.readline()
                if line[:1] in b'\xE0\xF0\xF8\xFC\xFE\xFF':
                    continue # yuck: ignore power-up noise from UART TX
            except:
                ok = False
                break
            if len(line) == 0:
                if not ok:
                    delay = 0
                break

            try:
                line = line.decode()
            except UnicodeDecodeError:
                line = "binary: " + line[:-1].hex() + "\n" # not utf8

            if line == "main\n":
                results = []
                ok = True
            if not ok and len(line) > 1:
                print("?", line[:-1])

            results.append(line)

            if line == "done\n":
                delay = 0.04
                break
            if line == "abort\n":
                ok = False
                delay = 0.3
                break

        time.sleep(delay)
        if not ok:
            fail += 1
        if compareWithExpected(fn, ''.join(results)):
            match += 1

    print(f"{tests} tests, {match} matches, "
          f"{fail} failures, {skip} skipped, {len(ignores)} ignored")
    if match != tests:
        sys.exit(1)
