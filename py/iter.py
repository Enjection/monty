a = iter(range(5))
print(type(a))
print(next(a))
print(next(a))
print(next(a))

b = (100+i*i for i in range(6))
print(type(b))
print(next(b))
print(next(b))
print(next(b))

print(type(iter(b)))

#for i in b:
#    print(i)

for i in (11,22,33):
    print(i)

c = [111,222,333]
print(type(iter(c)))
for i in c:
    print(i)

print(200)
print(tuple())
print(tuple((1,2,3)))
print(tuple([1,2,3]))
print(tuple({1,2,3}))
#FIXME print(tuple((i*i for i in range(6))))
print('(0,1,4,9,16,25)')

print(201)
print(list())
print(list((1,2,3)))
print(list([1,2,3]))
print(list({1,2,3}))
#FIXME print(list((i*i for i in range(6))))
print('[0,1,4,9,16,25]')

print(202)
print(set())
print(set((1,2,3)))
print(set([1,2,3]))
print(set({1,2,3}))
#FIXME print(set((i*i for i in range(6))))
print('{0,1,4,9,16,25}')

print(203)
print(dict())
print(dict(((1,11),(2,22),(3,33))))
print(dict([(1,11),(2,22),(3,33)]))
print(dict({1:11,2:22,3:33}))
#FIXME print(dict(((i,i*i) for i in range(6))))
print('{0:0,1:1,2:4,3:9,4:16,5:25}')
print(dict(a=1,b=2,c=3))
