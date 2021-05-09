from invoke import task

@task
def generate(c):
    "generate the Hall code for embedded board"
    # TODO call hallgen.py

@task
def test1(c):
    "native build & run, just says hello"
    c.run('pio run -e t1native')
    c.run('.pio/build/t1native/program')

@task
def test2(c):
    "upload, blinks the on-board LED"
    c.run('pio run -e t2blink')

@task
def test3(c):
    "upload, blinks using CMSIS & SysTick"
    c.run('pio run -e t3cmsis')

@task
def test4(c):
    "upload, blinks & sends to polled UART port"
    c.run('pio run -e t4uart', pty=True)

@task(generate)
def test5(c):
    "upload, blinks & sends to UART via Hall defs"
    c.run('pio run -e t5hall', pty=True)

@task(generate)
def test6(c):
    "upload, blinks & sends, adding printf from Boss"
    c.run('pio run -e t6printf', pty=True)

@task(generate)
def test7(c):
    "upload, blinks & sends using fibers to wait/suspend"
    c.run('pio run -e t7fiber', pty=True)

@task(generate)
def test8(c):
    "upload, blinks & sends using the DMA-based UART driver"
    c.run('pio run -e t8dma', pty=True)

@task(generate)
def test9(c):
    "upload, LED on when not idle, fast clock, continuous output"
    c.run('pio run -e t9idle', pty=True)

@task(generate)
def builds(c):
    "compile all embedded tests and show their sizes"
    c.run("pio run -e t2blink  -t size | tail -8 | head -2")
    c.run("pio run -e t3cmsis  -t size | tail -7 | head -1")
    c.run("pio run -e t4uart   -t size | tail -7 | head -1")
    c.run("pio run -e t5hall   -t size | tail -7 | head -1")
    c.run("pio run -e t6printf -t size | tail -7 | head -1")
    c.run("pio run -e t7fiber  -t size | tail -7 | head -1")
    c.run("pio run -e t8dma    -t size | tail -7 | head -1")
    c.run("pio run -e t9idle   -t size | tail -7 | head -1")

@task(generate, default=True)
def help(c):
    "default task, shows this help (same as 'inv -l')"
    c.run('inv -l')
