import sys
from machine import cycles as c

c() # throw away first result
print("cycles", [c(), c(c()), c(), c(print)])

def nooper():
    a = []
    for _ in range(10):
        a.append(c())
    print("nooper", a)

def waiter():
    for _ in range(9):
        yield

async def sender():
    a = []
    for _ in range(10):
        task.send()
        a.append(c())
    print("sender", a)

nooper()
task = waiter()
sys.ready.append(sender())
