#!/usr/bin/env python3

# Simple TDD watcher/runner - needs a matching makefile to work properly.
# Everything is configured via the makefile, launch this as: ./tdd.py
# This code relies on "python3", "make", "fswatch", and "doctest.h".
# -jcw, May 2021

import os, subprocess, sys, time

if len(sys.argv) < 2:
    print("<<< re-starting as: make tdd >>>")
    os.execlp('make', 'make', 'tdd')
    assert False

incMap, revMap, hdrMap, srcSet = {}, {}, {}, set()

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
        print("(U) NEW:  ", incs)
        print("(U) OLD:  ", prevIncs)
        print("(U)   DEL:", prevIncs - incs)
        print("(U)   ADD:", incs - prevIncs)

    for h in prevIncs - incs:
        revMap[h].remove(fn)
    for h in incs - prevIncs:
        revMap[h] = revMap.get(h, set())
        revMap[h].add(fn)
        if h in hdrMap and hdrMap[h] not in incMap:
            updateMaps(hdrMap[h]) # scan header if not done earlier

# remove object files which include this header (transitively)
def removeBuild(bn):
    if bn in revMap:
        for u in revMap[bn]:
            if u[-2:] == ".h":
                removeBuild(os.path.basename(u))
            elif u[-4:] == ".cpp":
                o = os.path.basename(u)[:-4] + '.o'
                if os.path.isfile(o):
                    if verbose:
                        print("(R) GONE:", o)
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
        print("(S) NEW:  ", files)
        print("(S) OLD:  ", srcSet)
        print("(S)   DEL:", srcSet - files)
        print("(S)   ADD:", files - srcSet)

    for s in srcSet - files:
        del incMap[s]
    for s in files - srcSet:
        updateMaps(s)
    srcSet.clear()
    srcSet.update(files)

# process changes in Makefile, *.h, *.cpp, and this script
def main():
    findHeaders()
    scanSources()
    print("<<<", len(hdrMap), "headers,", len(srcSet), "sources", ">>>")

    last = 0
    for line in proc.stdout:
        fn = line.strip()
        bn = os.path.basename(fn)
        ext = os.path.splitext(bn)[1]
        if bn == 'tdd.py':
            print(bn, "changed")
            sys.exit()

        if ext in ['','.h','.cpp']:
            if bn == 'Makefile':
                findHeaders()
            elif ext:
                updateMaps(fn)
                if ext == '.h':
                    removeBuild(bn)

            if time.monotonic() > last + 0.5:
                cmd = ['make', 'all']
                if bn == 'Makefile':
                    cmd.insert(1, 'clean')
                subprocess.run(cmd)
                scanSources()

            last = time.monotonic()

verbose = False
if sys.argv[1] == "-v":
    verbose = True
    del sys.argv[1]

cmd = ['fswatch', '-l', '0.1'] + sys.argv[1:]
print("<<< %s >>>" % ' '.join(cmd))
proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True)

os.environ.pop('MAKELEVEL', None) # don't generate sub-make output in gmake

try:
    main()
except KeyboardInterrupt:
    print()
    sys.exit(2)
