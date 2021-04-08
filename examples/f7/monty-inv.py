# see https://www.pyinvoke.org

@task(flash, default=True)
def all(c):
    """compile and upload the "f7" demo"""

@task
def disas(c):
    """show code disassembly"""
    c.run("arm-none-eabi-objdump -dC .pio/build/disco-f750/firmware.elf")

@task
def map(c):
    """show a memory map with symbols"""
    c.run("arm-none-eabi-nm -CnS .pio/build/disco-f750/firmware.elf |"
          "grep -v Handler")

@task(flash)
def gdb(c):
    """compile and run in debugger to capture swo output"""
    print("launch 'nc 127.0.0.1 6464' in separate window to see SWO output")
    c.run("arm-none-eabi-gdb", pty=True)

# remove irrelevant tasks
del mrfs, native, python, runner, serial, test, upload, watch
