# see https://www.pyinvoke.org

@task(flash, default=True)
def all(c):
    """compile and upload the "blinker" demo"""

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
