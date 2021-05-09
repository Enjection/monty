from invoke import task

@task
def generate(c):
    "generate the Hall code for embedded board"
    # TODO call hallgen.py

@task
def test1(c):
    "native build & run, just says hello"
    c.run('pio run -e s1native')
    c.run('.pio/build/s1native/program')

@task
def test2(c):
    "embedded upload, blinks the on-board LED"
    c.run('pio run -e s2blink')

@task
def test3(c):
    "embedded upload, blinks using CMSIS & SysTick"
    c.run('pio run -e s3cmsis')

@task
def test4(c):
    "embedded upload, blinks & sends to polled UART port"
    c.run('pio run -e s4uart', pty=True)

@task(generate)
def test5(c):
    "embedded upload, blinks & sends to UART via Hall defs"
    c.run('pio run -e s5hall', pty=True)

@task(generate)
def test6(c):
    "embedded upload, blinks & sends, adding printf from Boss"
    c.run('pio run -e s6printf', pty=True)

@task(generate)
def test7(c):
    "embedded upload, blinks & sends using DMA-based UART driver"
    c.run('pio run -e s7dma', pty=True)

@task(generate)
def test8(c):
    "embedded upload, blinks & sends using fibers to wait & suspend"
    c.run('pio run -e s8fiber', pty=True)

@task(generate)
def test9(c):
    "embedded upload, LED is on while not idle, continuous text output"
    c.run('pio run -e s9idle', pty=True)

@task(generate)
def builds(c):
    "compile all embedded tests and show their sizes"
    c.run("pio run -e s2blink  -t size | tail -8 | head -2")
    c.run("pio run -e s3cmsis  -t size | tail -7 | head -1")
    c.run("pio run -e s4uart   -t size | tail -7 | head -1")
    c.run("pio run -e s5hall   -t size | tail -7 | head -1")
    c.run("pio run -e s6printf -t size | tail -7 | head -1")
    c.run("pio run -e s7dma    -t size | tail -7 | head -1")
    c.run("pio run -e s8fiber  -t size | tail -7 | head -1")
    c.run("pio run -e s9idle   -t size | tail -7 | head -1")

@task(generate, default=True)
def help(c):
    "default task, shows this help (same as 'inv -l')"
    c.run('inv -l')
