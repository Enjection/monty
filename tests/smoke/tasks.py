from invoke import task

@task(default=True)
def help(c):
    "default task, amazingly useless"
    print('try: inv -l')

@task
def smoke1(c):
    "native build & run, just says hello"
    c.run('pio run -e smoke1')
    c.run('.pio/build/smoke1/program')

@task
def smoke2(c):
    "embedded upload, blink the on-board LED"
    c.run('pio run -e smoke2')

@task
def smoke3(c):
    "embedded upload, blink using CMSIS & SysTick"
    c.run('pio run -e smoke3')

@task
def smoke4(c):
    "embedded upload, send digits to polled UART port"
    c.run('pio run -e smoke4', pty=True)
