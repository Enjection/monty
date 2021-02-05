evt = machine.ticker(10)

def delay(n):
    for _ in range(n):
        evt.wait()
        evt.clear()

async def task(rate):
    i = 0
    while True:
        delay(rate)
        print('t', machine.ticks(), 'rate', rate, 'i', i)
        i += 1

for i in [2, 3, 5]:
    sys.tasks.append(task(i))

async def timeout():
    global evt
    delay(20)
    machine.ticker()

sys.tasks.append(timeout())
