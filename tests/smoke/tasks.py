from invoke import task

@task
def generate(c):
    "generate the Hall code for embedded board"
    # TODO call hallgen.py

@task
def test01(c):
    "native build & run, just says hello"
    c.run('pio run -e t1native')
    c.run('.pio/build/t01native/program')

@task
def test02(c):
    "upload, blinks the on-board LED"
    c.run('pio run -e t02blink')

@task
def test03(c):
    "upload, blinks using CMSIS & SysTick"
    c.run('pio run -e t03cmsis')

@task
def test04(c):
    "upload, blinks & sends to polled UART port"
    c.run('pio run -e t04uart', pty=True)

@task(generate)
def test05(c):
    "upload, blinks & sends to UART via Hall defs"
    c.run('pio run -e t05hall', pty=True)

@task(generate)
def test06(c):
    "upload, blinks & sends, adding printf from Boss"
    c.run('pio run -e t06printf', pty=True)

@task(generate)
def test07(c):
    "upload, blinks & sends using fibers to wait/suspend"
    c.run('pio run -e t07fiber', pty=True)

@task(generate)
def test08(c):
    "upload, blinks & sends using the DMA-based UART driver"
    c.run('pio run -e t08dma', pty=True)

@task(generate)
def test09(c):
    "upload, LED on when not idle, fast clock, continuous output"
    c.run('pio run -e t09idle', pty=True)

@task(generate)
def test10(c):
    "upload, blink multiple LEDs, one per fiber"
    c.run('pio run -e t10multi', pty=True)

@task(generate)
def builds(c):
    "compile all embedded tests and show their sizes"
    c.run("pio run -e t02blink  -t size | tail -8 | head -2")
    c.run("pio run -e t03cmsis  -t size | tail -7 | head -1")
    c.run("pio run -e t04uart   -t size | tail -7 | head -1")
    c.run("pio run -e t05hall   -t size | tail -7 | head -1")
    c.run("pio run -e t06printf -t size | tail -7 | head -1")
    c.run("pio run -e t07fiber  -t size | tail -7 | head -1")
    c.run("pio run -e t08dma    -t size | tail -7 | head -1")
    c.run("pio run -e t09idle   -t size | tail -7 | head -1")
    c.run("pio run -e t10multi  -t size | tail -7 | head -1")

@task(generate, default=True)
def help(c):
    "default task, shows this help (same as 'inv -l')"
    c.run('inv -l')
