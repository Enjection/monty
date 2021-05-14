#!/usr/bin/env python3

verbose = False

import os, subprocess, sys, time

cmd = "fswatch -l 0.1 . ../../lib"
proc = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE, text=True)
print("<<< %s >>>" % cmd)

incMap, useMap = {}, {}

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
    if verbose:
        print(fn)
        print("  NEW:  ", incs)
        print("  OLD:  ", prevIncs)
        print("    DEL:", prevIncs - incs)
        print("    ADD:", incs - prevIncs)
    for h in prevIncs - incs:
        useMap[h].remove(fn)
    for h in incs - prevIncs:
        useMap[h] = useMap.get(h, set())
        useMap[h].add(fn)
    incMap[fn] = incs

def removeBuild(bn):
    if bn in useMap:
        for u in useMap[bn]:
            if u[-2:] == ".h":
                removeBuild(os.path.basename(u))
            elif u[-4:] == ".cpp":
                o = os.path.basename(u)[:-4] + '.o'
                if os.path.isfile(o):
                    print("REMOVE", o)
                    os.remove(o)

try:
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
                    cmd.insert(1, 'clean')
                subprocess.run(cmd)
            last = time.monotonic()
except KeyboardInterrupt:
    print()
