# see https://www.pyinvoke.org

@task(flash, serial, default=True)
def all(c):
    """compile, upload, and connect to the demo"""

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
del mrfs, native, python, runner, test, upload, watch
