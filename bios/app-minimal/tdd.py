#!/usr/bin/env python3

# Simple TDD watcher/runner - needs a matching makefile to work properly.
# Everything is launched & configured via the makefile, launch as "make".
# This runs on MacOS/Linux and requires "python3", "make", and "fswatch".
# -jcw, May 2021

import os, subprocess, sys, time

# set up fswatch-based file-change watcher (MacOS & Linux), returns io stream
def setupWatcher():
    cmd = ['fswatch', '-l', '0.1']
    if sys.platform == 'linux':
        cmd += '-r --event Created --event Removed --event Updated'.split()
    cmd += sys.argv[1:]
    print("<<< %s >>>" % ' '.join(cmd))
    subprocess.run(['make', 'run'])
    return subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True).stdout


# process changes in Makefile, *.h, *.cpp, and this script
def main():
    last = 0
    for line in setupWatcher():
        bn = os.path.basename(line.strip())
        if bn == 'tdd.py':
            print(bn, "changed")
            return

        ext = os.path.splitext(bn)[1]
        if ext in ['','.h','.cpp']:
            if time.monotonic() > last + 0.5:
                cmd = ['make', 'all']
                if bn == 'Makefile':
                    cmd.insert(1, 'clean')
                subprocess.run(cmd)

            last = time.monotonic()

os.environ.pop('MAKELEVEL', None) # don't generate sub-make output in gmake

verbose = False
if sys.argv[1] == "-v":
    verbose = True
    del sys.argv[1]
assert len(sys.argv) >= 2

try:
    main()
except KeyboardInterrupt:
    print()
    sys.exit(1)
