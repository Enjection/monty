# see https://www.pyinvoke.org

@task(flash, mrfs, runner, default=True)
def all(c):
    """compile upload tests to the "bluepill" board"""
