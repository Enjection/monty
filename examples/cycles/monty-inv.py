# see https://www.pyinvoke.org

@task(flash, runner, default=True)
def all(c):
    """compile upload, and run tests on the Nucleo-L432 board"""
