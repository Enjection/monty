# this is used as post-build step in platformio.ini

import os, subprocess
Import("env")

currEnv = env["PIOENV"]
symFile = f"util/syms-{currEnv}.ld"

flashSegSize = 2048         # STM32L4-specific

# some globals need to be hidden from the next segment
hide = [
    '_Min_Stack_Size',
    '_Min_Heap_Size',
    '_rom_start_',
    '_rom_end_',
    '_ram_start_',
    '_ram_end_',
    '__bss_start__',
    '__bss_end__',
    '__exidx_start',
    '__exidx_end',
    '__libc_init_array',
    '__libc_fini_array',
    '_etext',
    '_init',
    '_fini',
    '_sidata',
    '_sdata',
    '_edata',
    '_sbss',
    '_ebss',
    '_siccmram',
    '_sccmram',
    '_eccmram',
    'g_pfnVectors',
    'init',
    'main',
    'SystemInit',
]

def nextMultipleOf(m, v):
    return v + (-v & (m-1)) # m must be a power of 2

def extract(source, target, env):
    elf = str(target[0])
    print(f"Extracting symbols to {symFile}")
    with os.popen(f"arm-none-eabi-readelf -s '{elf}'") as ifd:
        with open(symFile, "w") as ofd:
            print("/* auto-generated by pio-extra.py */", file=ofd)
            syms = {}
            for line in ifd:
                fields = line.split()
                if len(fields) > 4 and fields[4] == 'GLOBAL':
                    name = fields[7]
                    value = fields[1]
                    if name in hide:
                        syms[name] = int(value, 16)
                    else:
                        print(f"{name} = 0x{value};", file=ofd)

            # calculate and save flash + ram limits to skip in next segment
            romStart = syms["g_pfnVectors"]
            romEnd = nextMultipleOf(flashSegSize, syms["_sidata"])
            ramStart = syms["_sdata"]
            ramEnd = nextMultipleOf(8, syms["_ebss"])
            print(f"_rom_start_ = {romStart:#x};\n"
                  f"_rom_end_ = {romEnd:#x};\n"
                  f"_ram_start_ = {ramStart:#x};\n"
                  f"_ram_end_ = {ramEnd:#x};",
                  file=ofd)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", extract)

def padding(source, target, env):
    img = str(target[0])
    syms = {}
    with open(symFile) as fd:
        for line in fd:
            fields = line.split()
            if len(fields) == 3 and fields[1] == "=":
                k, v = fields[0], fields[2]
                syms[k] = int(v[-6:-1], 16) # use lower 20 bits of hex value
    start, limit = syms["_rom_start_"], syms["_rom_end_"]
    subprocess.run([env["OBJCOPY"], "-F", "binary", "--gap-fill", "0xFF",
                    "--pad-to", "%#x" % (limit-start), img])

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", padding)
