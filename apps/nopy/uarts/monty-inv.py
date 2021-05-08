# see https://www.pyinvoke.org

@task(flash, serial, default=True)
def all(c):
    """compile, upload, and connect to the nucleo-f413 board"""

del mrfs, native, python, runner, test, upload, watch
