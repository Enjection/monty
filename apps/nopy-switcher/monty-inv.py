# see https://www.pyinvoke.org

@task(flash, serial, default=True)
def all(c):
    """compile, upload, and run the "switcher" demo"""

# remove irrelevant tasks
del mrfs, native, python, runner, test, upload, watch
