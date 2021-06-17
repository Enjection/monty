#!/usr/bin/env python3
# This is Monty's code generator, driven by "//CG" and "Q()" directives.

import os, re, sys
from os import path

# node  = [[cmd, args...], offset, lineNum, block]
# text  = [None, offset, lineNum, block]
# nodes = [node|text, ...]
# tree  = {fpath: nodes, ...}

class Attrs: pass

def parse(text):
    # split the suppliedsource code into a list of "//CG..." and text nodes
    nodes, remaining, lineNum = [], 0, 0
    for l in text.split("\n"):
        lineNum += 1
        offset = l.find("//CG")
        if offset < 0:
            if remaining > 0:
                remaining -= 1
            elif remaining == 0 and (not nodes or nodes[-1][0]):
                nodes.append((None, l[:offset], lineNum, []))
            nodes[-1][-1].append(l)
        elif "//CG>" in l:
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
            nodes.append((cmd[2:].split(), l[:offset], lineNum, []))
    return nodes

def parseAll(paths):
    # load and parse all source files in the specified directories
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

def emit(nodes):
    # convert a list of nodes back to a multi-line string
    lines = []
    for cmd, prefix, lineNum, block in nodes:
        if cmd:
            n = len(block)
            if n > 3:
                head = '//CG<'
            elif n > 0:
                head = '//CG%d' % n
            else:
                head = '//CG:'
            lines.append(prefix + ' '.join([head, *cmd]))
            if block and block[0][:1].isspace():
                lines += block # don't re-indent
            else:
                for b in block:
                    lines.append(prefix + b)
            if head[-1] == "<":
                lines.append(prefix + "//CG>")
        else:
            lines += block
    return "\n".join(lines)

def emitAll(tree):
    # replace files with updated versions, but only if they changed
    for k, v in tree.items():
        out = emit(v.nodes)
        if out != v.text:
            print(k, len(v.text), len(out))
            with open(v.fpath, "w") as f:
                f.write(out)

def traverse(tree, state):
    # given a state object with methods in upper case, traverse the parse tree
    # and call the corresponding methods for each node if defined, or if there
    # is an "any_NODE" method, call that instead - the return value will be
    # used as generated output if non-null
    anyNode = getattr(state, "any_NODE", None)
    allText = getattr(state, "all_TEXT", None)
    for k, v in tree.items():
        state.fname = k
        for node in v.nodes:
            r = None
            if node[0]:
                cmd, *args = node[0]
                meth = getattr(state, cmd.upper().replace('-','_'), anyNode)
                if meth:
                    state.node = node
                    r = meth(node[-1], *args)
            elif allText:
                r = allText(node[-1])
            if r is not None:
                node[-1][:] = r
    return state

def opType(typ):
    if 'q' in typ:
        return ' %s', 'fetchQ()', 'Q arg'
    elif 'v' in typ:
        return ' %u', 'fetchV()', 'int arg'
    elif 'o' in typ:
        return ' %d', 'fetchO()', 'int arg'
    elif 's' in typ:
        return ' %d', 'fetchO()-0x8000', 'int arg'
    elif 'm' in typ:
        return ' %d', '_ip[-1]', 'uint32_t arg'
    return '', '', ''

class Expand:
    # expand directives which have no side-effects

    def ARGS(self, block, *args):
        # generate positional arg checks and conversions
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
        # bind a function, i.e. define a callable function object wrapper
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

    def BINOPS(self, block, fname, count):
        # parse the py/runtime0.h header
        out, count = [""], int(count)
        with open(fname, 'r') as f:
            for line in f:
                line = line.strip()
                if line.startswith('MP_BINARY_OP_'):
                    item = line.split()[0][13:].title().replace('_', '')
                    if len(out[-1]) + len(item) > 70:
                        out.append("")
                    if out[-1]:
                        out[-1] += " "
                    out[-1] += item
                    count -= 1
                    if count <= 0:
                        break
        return out

    def KWARGS(self, block, *args):
        # generate option parsing code, i.e. keyword args
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
        # expand opcode functions
        op = block[0].split()[1][2:]
        fmt, arg, decl = opType(typ)
        return ['void op%s (%s) {' % (op, decl)]

    def OPCODES(self, block, fname):
        # parse the py/bc0.h header
        defs, bases = [], {}
        with open(fname, 'r') as f:
            for line in f:
                if line.startswith('#define'):
                    fields = line.split()
                    if len(fields) > 3:
                        if fields[3] == '//':
                            bases['('+fields[1]] = '('+fields[2]
                        if fields[3] == '+':
                            fields[2] = bases[fields[2]]
                            val = eval(''.join(fields[2:5]))
                            key = fields[1][6:].title().replace('_', '')
                            defs.append((val, key))
        return ['%-22s = 0x%02X,' % (k, v) for v, k in sorted(defs)]

    def TYPE(self, block, tag, *args):
        # generate the header of an exposed type
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

class Qstr:
    # this must be called three times: setup, replace, emit varyvec data

    def __init__(self):
        self.qstrMap = {}    # map string to id
        self.qstrLen = []    # per id, the symbol length + 1
        self.phase = 0

    def step(self):
        self.phase += 1
        return self

    def QSTR(self, block, off="0"):
        # extract the initial qstrings, built into the VM
        if self.phase == 0:
            off, sep, pos = int(off), '"\\0"', 0
            for s in block:
                s = s.replace(sep, '')
                s = s.split('"')[-2]
                self.qstrMap[s] = off
                s = '"%s"' % s
                n = pos + len(eval(s)) + 1 # deal with backslashes
                off += 1
                self.qstrLen.append(n-pos)
                pos = n

    def q(self, s, a=None):
        # generate a qstr reference with the proper ID assigned and filled in
        if s in self.qstrMap:
            i = self.qstrMap[s]
        else:
            i = len(self.qstrMap) + 1
            self.qstrMap[s] = i
            self.qstrLen.append(len(s) + 1)
        return 'Q(%d,"%s")' % (i, s)

    def any_NODE(self, block, *args):
        # perform in-line qstr lookup and replacement
        if self.phase == 1:
            p = re.compile(r'\bQ\(\d+,"(.*?)"\)')
            out = []
            for s in block:
                out.append(p.sub(lambda m: self.q(m.group(1)), s))
            return out

    def all_TEXT(self, block):
        return self.any_NODE(block)

    def QSTR_EMIT(self, block):
        if self.phase == 2:
            # TODO
            print("qstr:", len(self.qstrMap), self.qstrLen[-1])

class Strip:
    # remove generated code where possible (some need to keep original code)
    keepAll = [ "binops", "exceptions", "if", "module", "opcodes", "qstr" ]
    keepOne = [ "bind", "op", "type", "wrap", "wrappers" ]
    def any_NODE(self, block, *args):
        cmd = self.node[0][0]
        if cmd in self.keepOne:
            return block[:1]
        if cmd not in self.keepAll:
            return []

class Stats:
    # collect statistics, i.e. the type and number of directives in the input
    def __init__(self):
        self.stats = {}
    def any_NODE(self, block, *args):
        cmd = self.node[0][0]
        self.stats[cmd] = self.stats.get(cmd, 0) + 1
    def dump(self):
        for k in sorted(self.stats):
            print(k, self.stats[k])

def main():
    # main application logic
    tree = parseAll(sys.argv[1:])
    for k, v in tree.items():
        n = len(v.nodes) // 2
        if n > 0:
            print(k, n)
    print()
    traverse(tree, Stats()).dump()
    print()
    traverse(tree, Expand())
    print()
    q = Qstr()
    traverse(tree, q)
    traverse(tree, q.step())
    traverse(tree, q.step())
    print()
    traverse(tree, Strip())
    emitAll(tree)

if __name__ == '__main__':
    main()
