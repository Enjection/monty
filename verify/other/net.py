print(monty.version)

network.ifconfig('192.168.188.2', '255.255.255.0', '192.168.188.1', '8.8.8.8')

s = network.socket()
print(s)
s.bind(1234)
s.listen(3)

async def onAccept(sess):
    print('onAccept', sess)
    sess.read(100)
    print('read1')
    sess.read(100)
    print('read2')
    while True:
        #sess.read(1000)
        sess.write('mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm\n')
        print('written')

s.accept(onAccept)

def loop():
    while True:
        network.poll()
        yield

machine.timer(10, loop())