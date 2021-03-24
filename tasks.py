# see https://www.pyinvoke.org

import configparser, os, sys
from invoke import task, call
from os import path
from src.runner import compileIfOutdated, compareWithExpected, printSeparator

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

@task(incrementable=["verbose"],
      help={"verbose": "print some extra debugging output (repeat for more)",
            "strip": "strip most generated code from the source files",
            "norun": "only report the changes, don't write them out"})
def generate(c, strip=False, verbose=False, norun=False):
    """pass source files through the code generator"""
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
    if strip and not root: # don't forget to strip all the examples
        c.run("""for d in `find examples -name monty-pio.ini`
                    do (cd `dirname $d` && inv generate -s -v); done""")

@task(call(generate, strip=True))
def clean(c):
    """delete all build results"""
    c.run("rm -rf .pio kcov-out/ src/__pycache__ verify/*.mpy")
    c.run("rm -rf examples/*/.pio examples/*/*/.pio")

@task(generate, default=not root,
      help={"file": "name of the .py or .mpy file to run"})
def native(c, file="verify/hello.py"):
    """run script using the native build  [verify/hello.py]"""
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
    """run Python tests natively          [in verify/: {*}.py]"""
    c.run(pio("run -e native -s"), pty=True)
    if dry:
        msg = tests or "verify/*.py"
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
        files = os.listdir("verify/")
        files.sort()
    for fn in files:
        if fn.endswith(".py") and fn[:-3] not in ignore:
            if not tests and fn[1:2] == "_" and fn[0] != 'n':
                skip += 1
                continue # skip non-native tests
            num += 1
            py = "verify/" + fn
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

@task(generate, help={"filter": 'filter tests by name (default: "*")'})
def test(c, filter='*'):
    """run C++ tests natively"""
    try:
        r = c.run(pio("test -e native -f '%s'" % filter),
                              hide='stdout', pty=True)
    except Exception as e:
        print(e.result.stdout)
    else:
        shortTestOutput(r)

@task(generate)
def flash(c):
    """build embedded and re-flash attached µC"""
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
    """run C++ tests, uploaded to µC"""
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
    """run Python tests, sent to µC       [in verify/: {*}.py]"""
    match = "{%s}" % tests if "," in tests else (tests or "*")
    iflag = ""
    if ignore:
        iflag = "-i " + ",".join(ignore)
    cmd = [inRoot("src/runner.py"), iflag, "verify/%s.py" % match]
    c.run(" ".join(cmd), pty=True)

@task(help={"offset": "flash offset (default: 0x0)",
            "file": "save to file instead of uploading to flash"})
def mrfs(c, offset=0, file=""):
    """upload tests as Minimal Replaceable File Storage image"""
    #c.run("cd lib/mrfs/ && g++ -std=c++17 -DTEST -o mrfs mrfs.cpp")
    #c.run("lib/mrfs/mrfs wipe && lib/mrfs/mrfs save verify/*.mpy" )
    mrfs = inRoot("src/mrfs.py")
    if file:
        c.run("%s -o %s verify/*.py" % (mrfs, file))
    else:
        c.run("%s -u %s verify/*.py" % (mrfs, offset), pty=True)

@task
def serial(c):
    """serial terminal session, use in separate window"""
    c.run(pio("run -t monitor -s"), pty=True)

@task(help={"file": "the Python script to send whenever it changes",
            "remote": "run script remotely iso natively"})
def watch(c, file, remote=False):
    """watch-exec/upload-print loop for quick Python iteration"""
    cmd = [inRoot("src/watcher.py")] + remote*["-r"] + [file]
    c.run(" ".join(cmd), pty=True)

# the following task is named differently when in-tree

@task(name=("env" if root else "x-env"),
      help={"env": "save new default PIO env, i.e. [env:<arg>]"})
def env(c, env):
    """change the default env to use in "monty-pio.ini" """
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
        # FIXME all sorts of hard-coded assumptions here ...
        with open("lib/arch-stm32/prelude.h", "w") as f:
            dev = "STM32L4x2"
            print('// this file was generated with "inv env %s"' % env, file=f)
            print('// do not edit, new versions will overwrite this file', file=f)
            print("\n//CG board %s -device %s" % (env, dev), file=f)
            sect = "config:%s" % env
            if sect in cfg and cfg[sect]:
                print("\n// from: [%s]" % sect, file=f)
                for k, v in cfg[sect].items():
                    print("//CG config %s" % " ".join([k] + v.split()), file=f)
            print(file=f)
            c.run("src/device.py %s" % dev, out_stream=f)
            print("\n// end of generated file", file=f)

if not root: # the following tasks are NOT available for use out-of-tree

    @task(call(generate, strip=True))
    def diff(c):
        """strip all sources and compare them to git HEAD"""
        c.run("git diff", pty=True)

    @task(generate)
    def builds(c):
        """show µC build sizes, w/ and w/o assertions or Python VM"""
        c.run("pio run -t size | tail -8 | head -2")
        c.run("pio run -e noassert | tail -7 | head -1")
        c.run("pio run -e nopyvm | tail -7 | head -1")

    @task(generate)
    def examples(c):
        """build each of the example projects"""
        examples = os.listdir("examples")
        examples.sort()
        for ex in examples:
            if path.isfile("examples/%s/README.md" % ex):
                print("building '%s' example" % ex)
                c.run("pio run -c platformio.ini -d examples/%s -t size -s" % ex,
                    warn=True)

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
        c.run("cp -a %s %s" % ("examples/template/", name))
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
    def x_tags(c):
        """update the (c)tags file"""
        c.run("ctags -R lib src test")

    @task
    def x_version(c):
        """display the current version tag (using "git describe)"""
        c.run("git describe --tags --always")

# don't use import but execute directly in this context - this lets tasks
# defined in "monty-inv.py" depend directly on all the tasks defined above
if path.isfile("monty-inv.py"):
    with open("monty-inv.py") as f:
        exec(f.read())
