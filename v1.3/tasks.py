# see https://www.pyinvoke.org

import configparser, json, os, sys
from invoke import task, call
from os import path

if not path.isfile("monty-pio.ini") and not path.isfile("platformio.ini"):
    if "--complete" not in sys.argv:
        print('No "monty-pio.ini" file found, cannot build from this directory.')
    sys.exit(1)

def getMontyDir():
    f = path.realpath(__file__)
    d = path.relpath(path.dirname(f), os.getcwd())
    return "" if d == "." else d

root = os.environ.get("MONTY_ROOT", "") or getMontyDir()
if root:
    os.environ["MONTY_ROOT"] = root
    sys.path.insert(0, root)

from src.runner import compileIfOutdated, compareWithExpected, printSeparator

dry = "-R" in sys.argv or "--dry" in sys.argv
exe = ".pio/build/native/program" # for native builds

def inRoot(f):
    return path.join(root, f)

def pio(args, *more):
    cmd = args.split(" ", 1)
    if root:
        cmd[1:1] = ["-c", inRoot("platformio.ini")]
    return " ".join(["pio"] + cmd + list(more))

# parse the platformio.ini file and any extra_configs it mentions
cfg = configparser.ConfigParser(interpolation=configparser.ExtendedInterpolation())
cfg.read_dict({"invoke": {}, "codegen": {}, "platformio": {}})
cfg.read(inRoot('platformio.ini'))
for f in cfg["platformio"].get("extra_configs", "").split():
    cfg.read(f)

python_ignore = cfg["invoke"].get("python_ignore", "").split()
runner_ignore = cfg["invoke"].get("runner_ignore", "").split()

# hard-coded paths, could be made configurable in the "[invoke]" section
preludeFile = inRoot("lib/arch-stm32/prelude.h")
boardDir = path.expanduser("~/.platformio/platforms/ststm32/boards/")

# find a config setting, recursively following the "extends = ..." chain(s)
def findSetting(name, sect):
    value = cfg[sect].get(name)
    if not value:
        for sect in cfg[sect].get("extends", "").split(", "):
            value = findSetting(name, sect)
            if value:
                break
    return value

# dig up the SVD file name from the board JSON file
def findSvdName(board):
    d = boardDir
    if path.isfile("boards/%s.json" % board):
        d = "boards"
    try:
        with open("%s/%s.json" % (d, board)) as f:
            data = json.load(f)
        svd = data["debug"]["svd_path"]
        return path.splitext(svd)[0]
    except (FileNotFoundError, KeyError):
        pass

# the start of the prelude must be as follows, else it should be updated
def preludePrefix(env, board, dev):
    out = [
        '// this file was generated with "inv env %s"' % env,
        '// do not edit, new versions will overwrite this file',
        '',
        '//CG: board %s -device %s -env %s' % (board, dev, env),
    ]
    sect = "config:%s" % env
    if sect in cfg and cfg[sect]:
        out.append("")
        out.append("// from: [%s]" % sect)
        for k, v in cfg[sect].items():
            out.append("//CG: config %s" % " ".join([k] + v.split()))
    out.append("")
    return "\n".join(out)

# messy: bypass codegen's expansion to allow comparing the corrent prelude
# this will also detect any changes to the "[config:<env>]" section
def stripGen(s):
    out = []
    for line in s.split("\n"):
        if len(line) > 5 and line.startswith("//CG"):
            out.append(line[5:])
    #print(out[:20])
    return "\n".join(out) # only the CG lines remain, without the CG? marker

# figure out whether the prelude matched the current environment, else update
# to speed up things, the src/device.py script is only run when really needed
def updatePrelude(c):
    env = cfg["platformio"].get("default_envs")
    if not env:
        return
    platform = findSetting("platform", "env:%s" % env)
    if platform != "ststm32":
        return;
    board = findSetting("board", "env:%s" % env)
    dev = findSvdName(board)
    if not dev:
        return
    prefix = preludePrefix(env, board, dev)
    if path.isfile(preludeFile):
        with open(preludeFile) as f:
            if stripGen(f.read()).startswith(stripGen(prefix)):
                return # nothing to update, they have the same prefix
    print("generating prelude.h for", dev)
    if not dry: # there's no way around it, run the src/device.py script
        with open(preludeFile, "w") as f:
            print(prefix, file=f)
            c.run(" ".join([inRoot("src/device.py"), dev]), out_stream=f)
            print("\n// end of generated file", file=f)

@task(incrementable=["verbose"],
      help={"verbose": "print some extra debugging output (repeat for more)",
            "strip": "strip most generated code from the source files",
            "norun": "only report the changes, don't write them out"})
def generate(c, strip=False, verbose=False, norun=False):
    """pass source files through the code generator"""
    if not strip or not root:
        updatePrelude(c)
    # construct codegen args from the [codegen] section in platformio.ini
    cmd = [inRoot("src/codegen.py")]
    cmd += strip*["-s"] + verbose*["-v"] + norun*["-n"]
    cmd += ["qstr.h", "lib/monty/"]
    cmd += cfg["codegen"].get("all", "").split()
    cmd += ["builtin.cpp"]
    for k, v in cfg["codegen"].items():
        if k != "all" and v:
            cmd += ["+" + k.upper()]
            cmd += v.split()
    cmd += ["qstr.cpp"]
    c.run(" ".join(cmd))
    if strip and not root: # don't forget to strip all the apps
        c.run("""for d in `find apps -name monty-pio.ini`
                    do (cd `dirname $d` && inv generate -s -v); done""")

@task(call(generate, strip=True))
def clean(c):
    """delete all build results"""
    c.run("rm -rf .pio kcov-out/ src/__pycache__ tests/py/*.mpy")
    c.run("rm -rf apps/*/.pio apps/*/*/.pio")

@task(generate, default=not root,
      help={"file": "name of the .py or .mpy file to run"})
def native(c, file="tests/py/hello.py"):
    """run script using the native build  [tests/py/hello.py]"""
    c.run(pio("run -e native -s"), pty=True)
    c.run("%s %s" % (exe, compileIfOutdated(file)))

def shortTestOutput(r):
    gen = (s for s in r.tail('stdout', 10).split("\n"))
    for line in gen:
        if line.startswith("==="):
            break
    for line in gen:
        if line:
            print(line)

@task(generate, iterable=["ignore"],
      help={"tests": "specific tests to run, comma-separated",
            "ignore": "one specific test to ignore",
            "coverage": "generate a code coverage report using 'kcov'"})
def python(c, ignore=[], coverage=False, tests=""):
    """run Python tests natively          [in tests/py/: {*}.py]"""
    c.run(pio("run -e native -s"), pty=True)
    if dry:
        msg = tests or "tests/py/*.py"
        print("# tasks.py: run and compare each test (%s)" % msg)
        return

    num, match, fail, skip, timeout, cmd = 0, 0, 0, 0, 2.5, [exe]
    if coverage:
        timeout = 60 # just in case
        cmd = ["kcov", "--collect-only", "kcov-out", exe]
        print("generating a code coverage report, this may take several minutes")
        c.run("rm -rf kcov-out")

    if tests:
        files = [t + ".py" for t in tests.split(",")]
    else:
        files = os.listdir("tests/py/")
        files.sort()
    for fn in files:
        if fn.endswith(".py") and fn[:-3] not in ignore:
            if not tests and fn[1:2] == "_" and fn[0] != 'n':
                skip += 1
                continue # skip non-native tests
            num += 1
            py = "tests/py/" + fn
            try:
                mpy = compileIfOutdated(py)
            except FileNotFoundError as e:
                printSeparator(py, e)
                fail += 1
            else:
                try:
                    r = c.run(" ".join(cmd + [mpy]), timeout=timeout, hide=True)
                except Exception as e:
                    r = e.result
                    msg = "[...]\n%s\n" % r.tail('stdout', 5).strip("\n")
                    if r.stderr:
                        msg += r.stderr.strip()
                    else:
                        msg += "Unexpected exit: %d" % r.return_code
                    printSeparator(py, msg)
                    fail += 1
                else:
                    if compareWithExpected(py, r.stdout):
                        match += 1

    if not dry:
        print(f"{num} tests, {match} matches, "
              f"{fail} failures, {skip} skipped, {len(ignore)} ignored")
    if coverage:
        c.run(" ".join(["kcov", "--report-only", "kcov-out", exe]))
        print("the coverage report is ready, see kcov-out/index.html")
    if num != match:
        c.run("exit 1") # yuck, force an error ...

@task(generate,
      help={"filter": 'filter tests by name (default: "*")',
            "unity": 'run Unity tests (default is DocTest)'})
def test(c, filter='*', unity=False):
    """run C++ tests natively"""
    if unity: # old unity tests
        try:
            r = c.run(pio("test -e native -f '%s'" % filter),
                                hide='stdout', pty=True)
        except Exception as e:
            print(e.result.stdout)
        else:
            shortTestOutput(r)
    else:
        c.run("cd tests/native && make clean main", hide=True)
        c.run("cd tests/native && ./main -nv", pty=True)

@task(generate)
def flash(c):
    """build embedded and re-flash attached ??C"""
    try:
        # only show output if there is a problem, not the std openocd chatter
        r = c.run(pio("run"), hide=True, pty=True)
        out = r.stdout.split("\n", 1)[0]
        if out:
            print(out) # ... but do show the target info
    except Exception as e:
        # captured as stdout due to pty, which allows pio to colourise
        print(e.result.stdout, end="", file=sys.stderr)
        sys.exit(1)

@task(help={"filter": 'filter tests by name (default: "*")'})
def upload(c, filter="*"):
    """run C++ tests, uploaded to ??C"""
    try:
        r = c.run(pio("test -f '%s'" % filter), hide='stdout', pty=True)
    except Exception as e:
        print(e.result.stdout)
    else:
        shortTestOutput(r)

@task(iterable=["ignore"],
      help={"tests": "specific tests to run, comma-separated",
            "ignore": "one specific test to ignore"})
def runner(c, ignore=[], tests=""):
    """run Python tests, sent to ??C       [in tests/py/: {*}.py]"""
    match = "{%s}" % tests if "," in tests else (tests or "*")
    iflag = ""
    if ignore:
        iflag = "-i " + ",".join(ignore)
    cmd = [inRoot("src/runner.py"), iflag, "tests/py/%s.py" % match]
    c.run(" ".join(cmd), pty=True)

@task(help={"offset": "flash offset (default: 0x0)",
            "file": "save to file instead of uploading to flash"})
def mrfs(c, offset=0, file=""):
    """upload tests as Minimal Replaceable File Storage image"""
    #c.run("cd lib/mrfs/ && g++ -std=c++17 -DTEST -o mrfs mrfs.cpp")
    #c.run("lib/mrfs/mrfs wipe && lib/mrfs/mrfs save tests/py/*.mpy" )
    mrfs = inRoot("src/mrfs.py")
    if file:
        c.run("%s -o %s tests/py/*.py" % (mrfs, file))
    else:
        c.run("%s -u %s tests/py/*.py" % (mrfs, offset), pty=True)

@task(help={"baud": "specify baudrate (default 921600)"})
def serial(c, baud=921600):
    """serial terminal session, use in separate window"""
    ini = root or "."
    c.run("pio device monitor -d %s -b %d --quiet" % (ini, baud), pty=True)

@task(help={"file": "the Python script to send whenever it changes",
            "remote": "run script remotely iso natively"})
def watch(c, file, remote=False):
    """watch-exec/upload-print loop for quick Python iteration"""
    cmd = [inRoot("src/watcher.py")] + remote*["-r"] + [file]
    c.run(" ".join(cmd), pty=True)

@task
def query(c, addr):
    """lookup failAt address in firmware build using addr2line"""
    env = cfg["platformio"].get("default_envs")
    elf = ".pio/build/%s/firmware.elf" %env
    print("in %s:\n  " % elf, end="")
    c.run("arm-none-eabi-addr2line -e %s %s" % (elf, addr));

# the following task is named differently when in-tree

@task(name=("env" if root else "x-env"),
      help={"env": "save new default PIO env, i.e. [env:<arg>]"})
def x_env(c, env):
    """change the default env in "monty-pio.ini" or "platformio.ini" """
    if "env:%s" % env not in cfg:
        print("unknown environment: [env:%s]" % env)
    else:
        prev, lines = "", []
        ini = "monty-pio.ini" if root else "platformio.ini"
        with open(ini) as f:
            for line in f:
                if line.startswith("default_envs = "):
                    prev = line.split()[-1]
                    line = "default_envs = %s\n" % env
                lines.append(line)
        if not prev:
            print('no "default_envs = ..." setting found')
        elif prev != env:
            print("switching to env:%s (was env:%s)" % (env, prev))
            cfg["platformio"]["default_envs"] = env
            if not dry:
                with open(ini, "w") as f:
                    f.write("".join(lines))

if not root: # the following tasks are NOT available for use out-of-tree

    @task(call(generate, strip=True))
    def diff(c):
        """strip all sources and compare them to git HEAD"""
        c.run("git diff", pty=True)

    @task(generate)
    def builds(c):
        """show ??C build sizes, w/ and w/o assertions or Python VM"""
        c.run("pio run -t size | tail -8 | head -2")
        c.run("pio run -e noassert | tail -7 | head -1")
        c.run("pio run -e nopyvm | tail -7 | head -1")

    @task(generate)
    def examples(c):
        """build each of the example projects"""
        for ex in sorted(os.listdir("apps/examples")):
            print("building '%s' example" % ex)
            c.run("cd apps/examples/%s && inv generate && "
                  "pio run -c ../../../platformio.ini -t size -s" % ex, warn=True)

    @task
    def health(c):
        """verify proper toolchain setup"""
        c.run("uname -sm")
        c.run("python3 --version")
        c.run("inv --version")
        c.run("pio --version")
        c.run("mpy-cross --version")
        #c.run("which micropython || echo NOT FOUND: micropython")

        fn = ".git/hooks/pre-commit"
        if root or path.isfile(fn):
            return
        print('creating pre-commit hook in "%s" for codegen auto-strip' % fn)
        if not dry:
            with open(fn, "w") as f:
                f.write("#!/bin/sh\ninv generate -s\ngit add -u .\n")
            os.chmod(fn, 0o755)

    @task(help={"name": "name of the new directory",
                "subdir": "subdir setup, omit symlink to 'tasks.py'"})
    def init(c, name, subdir=False):
        """intialise a new custom build area for Monty"""
        c.run("mkdir %s" % name)
        c.run("cp -a %s %s" % ("apps/template/", name))
        rel = path.relpath(".", name)
        if not dry:
            with open(path.join(name, "monty-pio.ini"), "w") as f:
                for s in [
                    "[platformio]",
                    "src_dir = %s" % path.join(rel, "src"),
                    "",
                    "[env]",
                    "lib_extra_dirs = %s" % path.join(rel, "lib"),
                ]:
                    print(s, file=f)
            if not subdir:
                t = "tasks.py"
                os.symlink(path.join(rel, t), path.join(name, t))
            print("Build area created, next step: cd %s && inv -l" % name)

    @task(post=[clean, test, call(python, python_ignore),
                upload, flash, mrfs, call(runner, runner_ignore),
                builds, examples])
    def all(c):
        """i.e. clean test python upload flash mrfs runner builds examples"""
        # make sure the JeeH library is not found locally, i.e. unset this env var
        os.environ.pop("PLATFORMIO_LIB_EXTRA_DIRS", None)

    @task
    def x_ctags(c):
        """update the (c)tags file"""
        c.run("ctags -R lib src tests")

    @task(generate, help={"file": "name of the .py or .mpy file to run"})
    def x_doctest(c, file="tests/py/hello.py"):
        """run native build including doctest [tests/py/hello.py]"""
        c.run(pio("run -e doctest -s"), pty=True)
        testexe = ".pio/build/doctest/program" # for native builds
        c.run("%s %s" % (testexe, compileIfOutdated(file)), pty=True)

    @task
    def x_tdd(c):
        """continuous TDD builds and test runs"""
        c.run("cd src && make tdd", pty=True)

    @task
    def x_version(c):
        """display the current version tag (using "git describe)"""
        c.run("git describe --tags --always")

# don't use import but execute directly in this context - this lets tasks
# defined in "monty-inv.py" depend directly on all the tasks defined above
if path.isfile("monty-inv.py"):
    with open("monty-inv.py") as f:
        exec(f.read())
