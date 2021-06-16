#!/usr/bin/env python3
# This is Monty's code generator, driven by "//CG" and "Q()" directives.

import os, sys
from os import path

def parse(text):
    nodes, remaining, lineNum = [], 0, 0
    for l in text.split("\n"):
        lineNum += 1
        i = l.find("//CG")
        if i < 0:
            if remaining > 0:
                remaining -= 1
            if remaining == 0 and (not nodes or nodes[-1][0]):
                nodes.append((None, lineNum, []))
            nodes[-1][-1].append(l)
        elif l[i:].startswith("//CG>"):
            assert remaining < 0
            remaining = 0
        else:
            assert remaining == 0
            cmd = l[i+4:]
            if cmd[:1].isdigit():
                remaining = int(cmd[0])
            elif cmd[:1] == "<":
                remaining = -1
            elif cmd[:1] != ":":
                cmd = ":" + cmd
            assert cmd[1].isspace()
            nodes.append((cmd[2:].split(), lineNum, []))
    for (a, b, c) in nodes:
        print(a, b, len(c))
    return nodes

def main():
    code = {}
    for a in sys.argv[1:]:
        if path.isdir(a):
            for f in sorted(os.listdir(a)):
                fpath = path.join(a, f)
                print(fpath)
                with open(fpath, "r") as f:
                    text = f.read()
                code[fpath] = parse(text)
    for k, v in code.items():
        print(k, len(v))

if __name__ == '__main__':
    main()
