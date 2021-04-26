# see https://www.pyinvoke.org

# make sure the JeeH library is not found locally, i.e. unset this env var
os.environ.pop("PLATFORMIO_LIB_EXTRA_DIRS", None)

env = cfg["platformio"].get("default_envs")

@task(flash, serial, default=True)
def all(c):
    """compile, upload, and attach serial"""

@task(generate)
def builds(c):
    """compile and show µC build sizes"""
    pio = "../../platformio.ini"
    c.run("pio run -c %s -t size -e blink | tail -8 | head -2" % pio)
    c.run("pio run -c %s -t size -e uart | tail -7 | head -1" % pio)

@task
def disas(c):
    """show code disassembly"""
    c.run("arm-none-eabi-objdump -dC .pio/build/%s/firmware.elf" % env)

@task
def map(c):
    """show a memory map with symbols"""
    c.run("arm-none-eabi-nm -CnS .pio/build/%s/firmware.elf |"
          "grep -v Handler" % env)

# remove irrelevant tasks
del mrfs, native, python, runner, serial, test, upload, watch
