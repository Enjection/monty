# see https://www.pyinvoke.org

@task(flash, default=True)
def all(c):
    """compile and upload to the "f7" demo"""

@task
def disas(c):
    """show code disassembly"""
    c.run("arm-none-eabi-objdump -dC .pio/build/disco-f750/firmware.elf")

@task
def map(c):
    """show a memory map with symbols"""
    c.run("arm-none-eabi-nm -CnS .pio/build/disco-f750/firmware.elf |"
          "grep -v Handler")

@task
def openocd(c):
    """launch openocd, ready for uploads and servinf SWO on port 6464"""
    print("launch 'nc 127.0.0.1 6464' in separate window to see SWO output")
    c.run("openocd -f board/stm32f7discovery.cfg"
          " -c 'tpiu config internal :6464 uart false 200000000 115200;"
               "itm port 0 on'", pty=True)

# remove irrelevant tasks
del mrfs, native, python, runner, test, upload, watch
