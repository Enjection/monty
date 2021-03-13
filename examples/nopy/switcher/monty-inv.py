# see https://www.pyinvoke.org

@task(flash, default=True)
def all(c):
    """compile and upload the "switcher" demo"""

# remove irrelevant tasks
del mrfs, native, python, runner, test, upload, watch
