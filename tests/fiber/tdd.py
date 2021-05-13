#!/usr/bin/env python3

import os, subprocess, time

cmd = "fswatch -l 0.1 . ../../lib"
proc = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE, text=True)
print("<<< %s >>>" % cmd)

last = 0
for line in proc.stdout:
    fn = line.strip()
    #print(fn)
    if os.path.splitext(fn)[1] in ['','.h','.cpp','.py']:
        if time.monotonic() > last + 0.3:
            subprocess.run("make")
        last = time.monotonic()
