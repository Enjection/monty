# see https://www.pyinvoke.org

@task
def debug(c):
    """enable BMP's SWO streaming"""
    c.run("arm-none-eabi-gdb .pio/build/bluepill/firmware.elf -q -batch --nx "
          "-ex 'tar ext %s' -ex 'mon trace'" % cfg["bmp"]["upload_port"])

@task(flash, debug, serial, default=True)
def all(c):
    """compile, upload, enable SWO, then connect over serial"""

del mrfs, native, python, runner, test, upload
