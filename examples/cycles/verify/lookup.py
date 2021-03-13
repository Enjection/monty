import sys
from machine import cycles as c

pslot = print

c() # throw away first result
print("cycles", c(), c(c()), c(), end="")
print("pslot", c(), c(pslot), c(pslot()), c(pslot())-c(pslot), end="")
print("pslot", c(), c(pslot), c(pslot()), c(pslot())-c(pslot), end="")
print("print", c(), c(print), c(print()), c(print())-c(print), end="")
print("print", c(), c(print), c(print()), c(print())-c(print), end="")

print()
