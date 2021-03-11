from machine import cycles as c

c() # throw away first result
print(c(), c())
print(c(), c())
print(c(), c())
print(c(), c())
print()

print('()', c(c()))
print()

print('(1)', c(c(),1))
print('(1,2)', c(c(),1,2))
print('(1,2,3)', c(c(),1,2,3))
print()

print('("a")', c(c(),"a"))
print('("a","b")', c(c(),"a","b"))
print('("a","b","b")', c(c(),"a","b","c"))
print()

v = 1
print('(v)', c(c(),v))
print('(v,v)', c(c(),v,v))
print('(v,v,v)', c(c(),v,v,v))
print()

print('(len("a"))', c(c(),len("a")))
print('(int(123))', c(c(),int(123)))
print('(int("123"))', c(c(),int("123")))
