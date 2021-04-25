# see https://www.pyinvoke.org

# make sure the JeeH library is not found locally, i.e. unset this env var
os.environ.pop("PLATFORMIO_LIB_EXTRA_DIRS", None)

@task(flash, default=True)
def all(c):
    """compile and upload code"""

@task(generate)
def build(c):
    """compile and show µC build size"""
    c.run("pio run -c ../../platformio.ini -t size | tail -8 | head -2")

@task
def disas(c):
    """show code disassembly"""
    c.run("arm-none-eabi-objdump -dC .pio/build/nucleo-l432/firmware.elf")

@task
def map(c):
    """show a memory map with symbols"""
    c.run("arm-none-eabi-nm -CnS .pio/build/nucleo-l432/firmware.elf |"
          "grep -v Handler")

# remove irrelevant tasks
del mrfs, native, python, runner, serial, test, upload, watch
