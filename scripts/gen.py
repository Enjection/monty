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

class Expander:

    def ARGS(self, block, *args):
        '''generate positional arg checks and conversions'''
        out, names, types, decls = [], [], "", {}
        for a in args:
            if a in ["?", "*"]:
                types += a
            else:
                n, t = (a + ":v").split(":")[:2]
                names.append(n)
                types += t
                if t not in decls:
                    decls[t] = []
                decls[t].append(n)
        tmap = {"v":"Value", "i":"int", "o":"Object", "s":"char const"}
        for t in decls:
            m = tmap[t]
            if t in "os":
                out += ["%s *%s;" % (m, ", *".join(decls[t]))]
            else:
                out += ["%s %s;" % (m, ", ".join(decls[t]))]
        params = ",&".join([""] + names)
        out += ['auto ainfo = args.parse("%s"%s);' % ("".join(types), params),
                "if (ainfo.isObj()) return ainfo;"]
        return out

    def BIND(self, block, fun, *args):
        '''bind a function, i.e. define a callable function object wrapper'''
        tmap = {"v":"Value", "i":"int", "o":"Object*", "s":"char const*"}
        out, params, types = [], [], ""
        for a in args:
            if a in ["?", "*"]:
                types += a
            else:
                n, t = (a + ":v").split(":")[:2]
                params.append("%s %s" % (tmap[t], n))
                types += t
        if "?" in types or "*" in types:
            params.insert(0, "ArgVec const& args")
        return ['static auto f_%s (%s) -> Value {' % (fun, ", ".join(params))]

    def KWARGS(self, block, *args):
        '''generate option parsing code, i.e. keyword args'''
        out = ["Value %s;" % ", ".join(args),
            "for (int i = 0; i < args.kwNum(); ++i) {",
            "    auto k = args.kwKey(i), v = args.kwVal(i);",
            "    switch (k.asQid()) {"]
        for a in args:
            out.append('        case Q(0,"%s"): %s = v; break;' % (a, a))
        out += ['        default: return {E::TypeError, "unknown option", k};',
                "    }",
                "}"]
        return out

    def OP(self, block, typ='', multi=0):
        '''expand opcode functions'''
        op = block[0].split()[1][2:]
        if 'q' in typ:
            fmt, arg, decl = ' %s', 'fetchQ()', 'Q arg'
        elif 'v' in typ:
            fmt, arg, decl = ' %u', 'fetchV()', 'int arg'
        elif 'o' in typ:
            fmt, arg, decl = ' %d', 'fetchO()', 'int arg'
        elif 's' in typ:
            fmt, arg, decl = ' %d', 'fetchO()-0x8000', 'int arg'
        elif 'm' in typ:
            fmt, arg, decl = ' %d', '_ip[-1]', 'uint32_t arg'
        else:
            fmt, arg, decl = '', '', ''
        return ['void op%s (%s) {' % (op, decl)]

    def TYPE(self, block, tag, *args):
        '''generate the header of an exposed type'''
        line = block[0].split()
        name, base = line[1], line[line.index(":")+1:-1]
        base = ' '.join(base) # accept multi-word, e.g. protected
        out = [
            block[0].strip(),
            '    static auto create (ArgVec const&,Type const* =nullptr) -> Value;',
            '    static Lookup const attrs;',
            '    static Type info;',
            '    auto type () const -> Type const& override { return info; }',
            '    void repr (Buffer&) const override;',
        ]
        if tag.startswith('<'):
            del out[1:3] # can't construct from the VM
            del out[-1]  # no need for default repr override
        #print(len(block), ":", args, self.fname)
        #print(block[0])
        return out

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
    traverse(tree, Expander())

if __name__ == '__main__':
    main()
