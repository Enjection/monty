#!/usr/bin/env python3

verbose = False

import os, subprocess, sys, time

cmd = "fswatch -l 0.1 . ../../lib"
proc = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE, text=True)
print("<<< %s >>>" % cmd)

incMap, useMap, hdrMap, srcs = {}, {}, {}, set()

# scan source files and update the include and reverse-include maps
def updateMaps(fn):
    incs = set()
    if os.path.isfile(fn):
        with open(fn) as f:
            for line in f.readlines():
                if line.startswith("#include"):
                    s = line.split('<')
                    if len(s) > 1:
                        s = s[1].split('>')[0]
                    else:
                        s = line.split('"')[1]
                    incs.add(s)

    prevIncs = incMap.get(fn, set())
    incMap[fn] = incs

    if verbose:
        print(fn)
        print("(U)  NEW:  ", incs)
        print("(U)  OLD:  ", prevIncs)
        print("(U)    DEL:", prevIncs - incs)
        print("(U)    ADD:", incs - prevIncs)
    for h in prevIncs - incs:
        useMap[h].remove(fn)
    for h in incs - prevIncs:
        useMap[h] = useMap.get(h, set())
        useMap[h].add(fn)
        if h in hdrMap and hdrMap[h] not in incMap:
            updateMaps(hdrMap[h]) # scan header if not done earlier

# remove object files which include this header (transitively)
def removeBuild(bn):
    if bn in useMap:
        for u in useMap[bn]:
            if u[-2:] == ".h":
                removeBuild(os.path.basename(u))
            elif u[-4:] == ".cpp":
                o = os.path.basename(u)[:-4] + '.o'
                if os.path.isfile(o):
                    if verbose:
                        print("(R)  GONE:", o)
                    os.remove(o)

# locate headers in all include directories
def findHeaders():
    hdrMap.clear()
    for d in subprocess.getoutput("make dirs").split():
        for f in os.listdir(d):
            if f[-2:] == '.h':
                hdrMap[f] = os.path.join(d, f)
    if verbose:
        for k in sorted(hdrMap):
            print("[H]", k, "=", hdrMap[k])

# pick up source file additions and deletions
def scanSources():
    files = set(subprocess.getoutput("make srcs").split())
    if verbose:
        print("(S)  NEW:  ", files)
        print("(S)  OLD:  ", srcs)
        print("(S)    DEL:", srcs - files)
        print("(S)    ADD:", files - srcs)
    for s in srcs - files:
        del incMap[s]
    for s in files - srcs:
        updateMaps(s)
    srcs.clear()
    srcs.update(files)

def main():
    last = 0
    for line in proc.stdout:
        fn = line.strip()
        bn = os.path.basename(fn)
        ext = os.path.splitext(fn)[1]
        if bn == 'tdd.py':
            print(bn, "changed")
            sys.exit()

        if ext in ['','.h','.cpp']:
            if time.monotonic() > last + 0.3:
                if ext:
                    updateMaps(fn)
                if ext == '.h':
                    removeBuild(bn)

                cmd = ['make', 'all']
                if bn == 'Makefile':
                    findHeaders()
                    cmd.insert(1, 'clean')
                subprocess.run(cmd)

                scanSources()

            last = time.monotonic()

try:
    findHeaders()
    scanSources()
    print("<<<", len(hdrMap), "headers,", len(srcs), "sources", ">>>")
    main()
except KeyboardInterrupt:
    print()
