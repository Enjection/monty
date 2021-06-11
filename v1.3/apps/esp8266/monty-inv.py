# see https://www.pyinvoke.org

@task(flash, runner, default=True)
def all(c):
    """compile for ESP8266 and run remote Python tests"""

del mrfs, native, python, test
