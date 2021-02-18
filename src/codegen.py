#!/usr/bin/env python3

import os, re, sys, subprocess

verbose = 0

funs = {"": []}
meths = {"": []}

def MODULE(block, mod):
    flags["mod"] = mod
    funs[mod] = []
    meths[mod] = []
    return []

def BIND(block, fun):
    mod = flags["mod"]
    funs[mod].append(fun)
    return ["static auto f_%s (ArgVec const& args) -> Value {" % fun]

def WRAP(block, typ, *methods):
    if typ not in funs:
        funs[typ] = []
        meths[typ] = []
    for m in methods:
        meths[typ].append(m)
    return block

def WRAPPERS(block):
    mod = flags["mod"]
    out = []

    funs[mod].sort()
    for f in funs[mod]:
        out.append("static Function const fo_%s (f_%s);" % (f, f))

    if mod:
        types = [mod]
    else:
        types = list(meths.keys())
        types.sort()

    fmt1 = "static auto m_%s_%s = Method::wrap(&%s::%s);"
    fmt2 = "static Method const mo_%s_%s (m_%s_%s);"
    for t in types:
        l = t.lower()
        meths[t].sort()
        for f in meths[t]:
            out.append("")
            out.append(fmt1 % (l, f, t, f))
            out.append(fmt2 % (l, f, l, f))
        if t == "":
            continue
        out.append("")
        out.append("static Lookup::Item const %s_map [] = {" % l)
        for f in funs[t]:
            if mod:
                out.append("    { %s, fo_%s }," % (q(f), f))
            else:
                out.append("    { %s, fo_%s_%s }," % (q(f), l, f))
        for f in meths[t]:
            out.append("    { %s, mo_%s_%s }," % (q(f), l, f))
        if mod == "":
            out.append("};")
            out.append("Lookup const %s::attrs (%s_map, sizeof %s_map);" % (t, l, l))

    del funs[mod]
    del meths[mod]
    return out

excIds = {}
excHier = []
excFuns = []
excDefs = []

def EXCEPTIONS(block):
    out = []
    for line in block:
        id = len(excHier)
        name, _, base = line.split()
        name = name.strip(",")
        baseId = -1 if base == '-' else excIds[base]
        excIds[name] = id
        excHier.append('{ %-29s %2d }, // %2d -> %s' % (q(name) + ",", baseId, id, base))
        excFuns.append('static auto e_%s (ArgVec const& args) -> Value {' % name)
        excFuns.append('    return Exception::create(E::%s, args);' % name)
        excFuns.append('}')
        excFuns.append('static Function const fo_%s (e_%s);' % (name, name))
        excDefs.append('{ %-29s fo_%s },' % (q(name) + ",", name))
        out.append("%-20s // %s" % (name + ",", base))
    return out

def EXCEPTION_EMIT(block, sel='h'):
    if sel == 'h':
        return excHier
    if sel == 'f':
        return excFuns
    if sel == 'd':
        return excDefs

def VERSION(block):
    v = subprocess.getoutput('git describe --tags')
    return ['constexpr auto VERSION = "%s";' % v]

# generate qstr definition
qstrIndex = []
qstrLen = []
qstrMap = {}

def qid(s):
    if s in qstrMap:
        i = qstrMap[s]
    else:
        i = len(qstrMap) + 1
        qstrMap[s] = i
        qstrLen.append(len(s) + 1)
    return i

def q(s):
    return 'Q(%3d,"%s")' % (qid(s), s)

def hash(s):
    h = 5381
    for b in s.encode():
        h = (((h<<5) + h) ^ b) & 0xFFFFFFFF
    return h

def QSTR_EMIT(block, sel='i'):
    if sel == 'i':
        return qstrIndex
    out = []
    num = len(qstrLen)
    qstrLen.insert(0, num)
    qstrLen.append(0)
    num += 2
    if sel in 'xv':
        i, n, s = 0, 2 * num, ''
        for x in qstrLen:
            s += '\\x%02X\\x%02X' % (n & 0xFF, n >> 8)
            i += 1
            n += x
            if i % 8 == 0:
                out.append('"%s"' % s)
                s = ''
        if s:
            out.append('"%s"' % s)
        out.append('// index [0..%d], hashes [%d..%d], %d strings [%d..%d]' %
                   (2*i-1, 2*i, 2*i+num-3, i-2, 2*i+num-2, n-1))
    if sel in 'hv':
        i, s, h = 0, '', {}
        for x in map(hash, qstrMap):
            s += '\\x%02X' % (x & 0xFF)
            i += 1
            h[x & 0xFF] = True
            if i % 16 == 0:
                out.append('"%s"' % s)
                s = ''
        if s:
            out.append('"%s"' % s)
        out.append('// found %d distinct hashes' % len(h))
    if sel in 'sv':
        e = ['%-22s "\\0" // %d' % ('"%s"' % k, v) for k, v in qstrMap.items()]
        out.extend(e)
    return out

def QSTR(block, off=0):
    out = []
    qstrIndex.clear()
    qstrIndex.append("   0, // %d" % off)
    sep='"\\0"'
    pos = 0
    for s in block:
        s = s.replace(sep, '')
        s = s.split('"')[-2]
        qstrMap[s] = off
        s = '"%s"' % s
        out.append('%-21s %s // %d' % (s, sep, off))
        n = pos + len(eval(s)) + 1 # deal with backslashes
        off += 1
        qstrIndex.append("%4d, // %d" % (n, off))
        qstrLen.append(n-pos)
        pos = n
    return out

# generate size printers
def SIZES(block, *args):
    fmt = 'printf("%%4d = sizeof %s\\n", (int) sizeof (%s));'
    return [fmt % (t, t) for t in args]

# generate a right-side comment tag
def TAG(block, *args):
    text = ' '.join(args)
    return [(76-len(text)) * ' ' + '// ' + text]

# generate builtin function table
types = [[], []]

# generate the header of an exposed type
def TYPE(block, tag, *_):
    line = block[0].split()
    name, base = line[1], line[3:-1]
    base = ' '.join(base) # accept multi-word, e.g. protected
    out = [
        'struct %s : %s {' % (name, base),
        '    static auto create (ArgVec const&,Type const* =nullptr) -> Value;',
        '    static Lookup const attrs;',
        '    static Type info;',
        '    auto type () const -> Type const& override { return info; }',
        '    auto repr (Buffer&) const -> Value override;',
    ]
    if tag.startswith('<'):
        types[0].append([tag,name,base])
        del out[1:3] # can't construct from the VM
        del out[-1]  # no need for default repr override
    else:
        types[1].append([tag,name,base])
    return out

# emit the info definitions
def TYPE_INFO(block):
    out = []
    types[0].sort()
    for tag, name, base in types[0]:
        out.append('Type %12s::info (%s);' % (name, q(tag)))
    out.append("")
    types[1].sort()
    fmt = 'Type %8s::info (%-15s, %6s::create, &%s::attrs);'
    for tag, name, base in types[1]:
        out.append(fmt % (name, q(tag), name, name))
    return out

# generate builtin function attributes
def TYPE_BUILTIN(block):
    out = []
    types[1].sort()
    for tag, name, base in types[1]:
            out.append('{ %-15s %s::info },' % (q(tag) + ",", name))
    return out

# enable/disable flags for current file
flags = {}

def ON(block, *args):
    for f in args:
        flags[f] = True

def OFF(block, *args):
    for f in args:
        if hasattr(flags, f):
            del flags[f]

# generate opcode switch entries
opdefs = []
opmulti = []

def OP_INIT(block):
    opdefs.clear()
    opmulti.clear()
def OP_EMIT(block, sel=0):
    if sel == 'd':
        return opdefs
    if sel == 'm':
        return opmulti

def OP(block, typ='', multi=0):
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
        fmt, arg, decl = ' %d', 'ip[-1]', 'uint32_t arg'
    else:
        fmt, arg, decl = '', '', ''
    name = 'op' + op

    if 'm' in typ:
        opmulti.append('if ((uint32_t) (ip[-1] - Op::%s) < %d) {' % (op, multi))
        opmulti.append('    %s = ip[-1] - Op::%s;' % (decl, op))
        if 'op:print' in flags:
            opmulti.append('    printf("%s%s\\n", (int) arg);' % (op, fmt))
        opmulti.append('    %s(arg);' % name)
        opmulti.append('    break;')
        opmulti.append('}')
    else:
        opdefs.append('case Op::%s:%s' % (op, ' {' if arg else ''))
        if arg:
            opdefs.append('    %s = %s;' % (decl, arg))
        info = ', arg' if arg else ''
        if 'op:print' in flags:
            if fmt == ' %s': info = ', (char const*) arg' # convert from qstr
            if fmt == ' %u': info = ', (unsigned) arg' # fix 32b vs 64b
            opdefs.append('    printf("%s%s\\n"%s);' % (op, fmt, info))
        opdefs.append('    %s(%s);' % (name, 'arg' if arg else ''))
        if 's' in typ:
            opdefs.append('    loopCheck(arg);')
        opdefs.append('    break;')
        if arg:
            opdefs.append('}')

    out = ['void %s (%s) {' % (name, decl)]

    return out

# parse the py/runtime0.h header
def BINOPS(block, fname, count):
    out = []
    with open(fname, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('MP_BINARY_OP_'):
                out.append(line.split()[0][13:].title().replace('_', ''))
                if len(out) >= count:
                    break
    return out

# parse the py/bc0.h header
def OPCODES(block, fname):
    defs = []
    bases = {}
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
    defs.sort()
    return ['%-22s = 0x%02X,' % (k, v) for v, k in defs]

# used to test codegen parameter handling
def ARGS(block, *args, **kwargs):
    out = []
    if args: out.append('// %s' % repr(args))
    if kwargs: out.append('// %s' % repr(kwargs))
    return out

# perform simple shell-like parsing of '-option value' pairs in the arg list
def parseArgs(params):
    args = []
    kwargs = {}
    it = iter(params)
    for p in it:
        v = p
        if p.startswith('-'):
            v = next(it)
        try: v = int(v)
        except ValueError:
            try: v = float(v)
            except ValueError: pass
        if p.startswith('-'):
            kwargs[p[1:]] = v
        else:
            args.append(v)
    return args, kwargs

# loop over all source lines and generate inline code at each //CG mark
def processLines(lines):
    result = []
    for line in lines:
        if line.strip()[0:4] != '//CG':
            result.append(line)
        else:
            if verbose:
                print(line) # TODO report line numbers
            request, *comment = line.split('#', 1)
            head, tag, *params = request.split()
            block = []
            if len(head) == 4:
                pass
            elif head[4] == ':':
                pass
            elif head[4] == '<':
                while True:
                    s = next(lines)
                    if s.strip()[0:4] == '//CG':
                        if s.strip()[4] != '>':
                            raise 'missing //CG>, got: ' + s
                        break
                    block.append(s)
            else:
                count = int(head[4:])
                for _ in range(count):
                    s = next(lines)
                    if s[0:4] == '//CG':
                        raise 'unexpected //CG, got: ' + s
                    block.append(s)

            name = tag.replace('-','_').upper()
            if 'x' in flags and 'x'+name in globals():
                name = 'x'+name
            try:
                handler = globals()[name]
            except:
                print('unknown tag:', tag)
                return

            n = line.find('//CG')
            prefix = line[:n]

            args, kwargs = parseArgs(params)
            try:
                replacement = handler(block, *args, **kwargs)
            except FileNotFoundError:
                replacement = None
                print('not found, keep as is:', args[0])
            if replacement is None:
                prefix = ''
            else:
                block = replacement

            if len(block) > 3:
                head = '//CG<'
            elif len(block) > 0:
                head = '//CG%d' % len(block)
            else:
                head = '//CG:'
            out = ' '.join([head, tag, *params])
            if comment:
                out += ' # ' + comment[0].strip()

            result.append(line[:n] + out)
            for s in block:
                result.append(prefix + s)
            if head == '//CG<':
                result.append(line[:n] + '//CG>')

    # perform in-line qstr lookup and replacement
    def qfix(m):
        return q(m.group(1))

    p = re.compile(r'\bQ\([ \d]*\d,"(.*?)"\)')
    return [p.sub(qfix, line) for line in result]

# process one source file, replace it only if the new contents is different
def processFile(d, f):
    path = os.path.join(d, f)
    flags.clear()
    flags["mod"] = ""
    if '/x' in path: # new code style
        flags['x'] = True
    if verbose:
        print(path)
    # TODO only process files if they have changed
    with open(path, 'r') as f:
        lines = [s.rstrip('\r\n') for s in f]
    result = processLines(iter(lines))
    if result and result != lines:
        print('rewriting:', path)
        with open(path, 'w') as f: # FIXME not safe
            for s in result:
                f.write(s+'\n')

if __name__ == '__main__':
    # args should be: first-files* root-dir last-files*
    first, root, last = [], None, []
    for f in sys.argv[1:]:
        if os.path.isdir(f):
            root = f
        elif not root:
            first.append(f)
        else:
            last.append(f)
    assert root, "no directory arg found"
    # process the first files
    for f in first:
        processFile(root, f)
    # process all files not listed as first or last
    files = os.listdir(root)
    files.sort();
    for f in files:
        if not f in first and not f in last:
            if os.path.splitext(f)[1] in ['.h', '.c', '.cpp']:
                processFile(root, f)
    # process the last files
    for f in last:
        processFile(root, f)
