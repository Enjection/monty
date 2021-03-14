# see https://www.pyinvoke.org

@task(native, flash, serial, default=True)
def all(c):
    """compile and run the "bytes" demo, native and embedded"""

# remove irrelevant tasks
del mrfs, python, runner, test, upload, watch
