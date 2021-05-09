from invoke import task

@task
def generate(c):
    "generate the Hall code for selected target"
    # TODO call hallgen.py

@task
def smoke1(c):
    "native build & run, just says hello"
    c.run('pio run -e s1native')
    c.run('.pio/build/s1native/program')

@task
def smoke2(c):
    "embedded upload, blinks the on-board LED"
    c.run('pio run -e s2blink')

@task
def smoke3(c):
    "embedded upload, blinks using CMSIS & SysTick"
    c.run('pio run -e s3cmsis')

@task
def smoke4(c):
    "embedded upload, blinks & sends to polled UART port"
    c.run('pio run -e s4uart', pty=True)

@task(generate)
def smoke5(c):
    "embedded upload, blinks & sends to UART using Hall defs"
    c.run('pio run -e s5hall', pty=True)

@task(generate)
def builds(c):
    "compile all embedded tests and show their sizes"
    c.run("pio run -e s2blink -t size | tail -8 | head -2")
    c.run("pio run -e s3cmsis -t size | tail -7 | head -1")
    c.run("pio run -e s4uart  -t size | tail -7 | head -1")
    c.run("pio run -e s5hall  -t size | tail -7 | head -1")

@task(generate, default=True)
def help(c):
    "default task, amazingly useless"
    print('try: inv -l')
