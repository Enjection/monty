# see https://www.pyinvoke.org

# make sure the JeeH library is not found locally, i.e. unset this env var
os.environ.pop("PLATFORMIO_LIB_EXTRA_DIRS", None)

env = cfg["platformio"].get("default_envs")

@task(flash, default=True)
def all(c):
    """compile and upload"""

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

@task
def native(c, file="verify/hello.py"):
    """compile and run natively"""
    c.run(pio("run -e native -s"), pty=True)
    c.run(exe)

@task
def openocd(c):
    """launch openocd, ready for uploads and serving SWO on port 6464"""
    print("launch 'nc 127.0.0.1 6464' in separate window to see SWO output")
    cmd = [
        "openocd",
        "-f board/st_nucleo_l4.cfg",
        "-c 'tpiu config internal :6464 uart false 80000000 115200'",
    ]
    c.run(" ".join(cmd), pty=True)

# remove irrelevant tasks
del mrfs, python, runner, test, upload, watch
