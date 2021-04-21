# see https://www.pyinvoke.org

@task(generate, default=True,
      help={"file": "input file with lines of text to parse"})
def run(c, file="in.txt"):
    """compile and run input parser on specified file"""
    c.run("pio run -c ../../platformio.ini -e native -s", pty=True)
    c.run(".pio/build/native/program %s" % file, pty=True)

# remove irrelevant tasks
del flash, mrfs, native, python, query, runner, serial, test, upload
