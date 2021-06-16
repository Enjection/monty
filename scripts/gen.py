#!/usr/bin/env python3
# This is Monty's code generator, driven by "//CG" and "Q()" directives.

import os, sys
from os import path

# node  = [[cmd, args...], offset, lineNum, block]
# text  = [None, offset, lineNum, block]
# nodes = [node|text, ...]
# tree  = {fpath: nodes, ...}

class Attrs: pass

def parse(text):
    '''Split source text into a list of "//CG..." and text nodes'''
    nodes, remaining, lineNum = [], 0, 0
    for l in text.split("\n"):
        lineNum += 1
        offset = l.find("//CG")
        if offset < 0:
            if remaining > 0:
                remaining -= 1
            elif remaining == 0 and (not nodes or nodes[-1][0]):
                nodes.append((None, offset, lineNum, []))
            nodes[-1][-1].append(l)
        elif l[offset:].startswith("//CG>"):
            assert remaining < 0
            remaining = 0
        else:
            assert remaining == 0
            cmd = l[offset+4:]
            if cmd[:1].isdigit():
                remaining = int(cmd[0])
            elif cmd[:1] == "<":
                remaining = -1
            elif cmd[:1] != ":":
                cmd = ":" + cmd
            assert cmd[1].isspace()
            nodes.append((cmd[2:].split(), offset, lineNum, []))
    return nodes

def parseAll(paths):
    tree = {}
    for p in paths:
        if path.isdir(p):
            for f in sorted(os.listdir(p)):
                fpath = path.join(p, f)
                #print(fpath)
                src = Attrs()
                src.fpath = fpath
                with open(fpath, "r") as f:
                    src.text = f.read()
                src.nodes = parse(src.text)
                #for x in src.nodes:
                #    print(*x[:-1], len(x[-1]))
                tree[fpath] = src
    return tree

def traverse(tree, state):
    fallback = getattr(state, "anyNode", None)
    for k, v in tree.items():
        state.fname = k
        for node in v.nodes:
            if node[0]:
                cmd, *args = node[0]
                meth = getattr(state, cmd.upper(), fallback)
                if meth:
                    state.node = node
                    r = meth(node[-1], *args)
                    if r:
                        node[-1][:] = r
    return state

class Ops:
    def ARGS(self, block, *args):
        print(args, self.fname)
        print(block[0])
    def TYPE(self, block, *args):
        print(len(block), ":", args, self.fname)
        print(block[0])

class Stats:
    def __init__(self):
        self.stats = {}
    def anyNode(self, block, *args):
        cmd = self.node[0][0]
        self.stats[cmd] = self.stats.get(cmd, 0) + 1
    def dump(self):
        for k in sorted(self.stats):
            print(k, self.stats[k])

def main():
    tree = parseAll(sys.argv[1:])
    for k, v in tree.items():
        print(k, len(v.nodes))
    print()
    traverse(tree, Stats()).dump()
    print()
    traverse(tree, Ops())

if __name__ == '__main__':
    main()
