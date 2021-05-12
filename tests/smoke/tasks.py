from invoke import task

@task
def generate(c):
    "generate the Hall code for embedded board"
    # TODO call hallgen.py

@task
def t0(c):
    "native build & run, just says hello"
    c.run('pio run -e t0native')
    c.run('.pio/build/e0native/program')

@task
def t1(c):
    "embedded, blinks the on-board LED"
    c.run('pio run -e t1blink')

@task
def t2(c):
    "embedded, blinks using CMSIS & SysTick"
    c.run('pio run -e t2cmsis')

@task
def t3(c):
    "embedded, blinks & sends to polled UART port"
    c.run('pio run -e t3uart', pty=True)

@task(generate)
def t4(c):
    "embedded, blinks & sends to UART via Hall defs"
    c.run('pio run -e t4hall', pty=True)

@task(generate)
def t5(c):
    "embedded, blinks & sends, adding printf from Boss"
    c.run('pio run -e t5printf', pty=True)

@task(generate)
def t6(c):
    "embedded, blinks & sends using fibers to wait/suspend"
    c.run('pio run -e t6fiber', pty=True)

@task(generate)
def t7(c):
    "embedded, blinks & sends using the DMA-based UART driver"
    c.run('pio run -e t7dma', pty=True)

@task(generate)
def t8(c):
    "embedded, LED on when not idle, fast clock, continuous output"
    c.run('pio run -e t8idle', pty=True)

@task(generate)
def t9(c):
    "embedded, blink multiple LEDs, one per fiber"
    c.run('pio run -e t9multi', pty=True)

@task(generate)
def builds(c):
    "compile all embedded tests and show their sizes"
    c.run("pio run -e t1blink  -t size | tail -8 | head -2")
    c.run("pio run -e t2cmsis  -t size | tail -7 | head -1")
    c.run("pio run -e t3uart   -t size | tail -7 | head -1")
    c.run("pio run -e t4hall   -t size | tail -7 | head -1")
    c.run("pio run -e t5printf -t size | tail -7 | head -1")
    c.run("pio run -e t6fiber  -t size | tail -7 | head -1")
    c.run("pio run -e t7dma    -t size | tail -7 | head -1")
    c.run("pio run -e t8idle   -t size | tail -7 | head -1")
    c.run("pio run -e t9multi  -t size | tail -7 | head -1")

@task(generate, default=True)
def help(c):
    "default task, shows this help (same as 'inv -l')"
    c.run('inv -l')
