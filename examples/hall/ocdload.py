#!/usr/bin/env python3

import socket, sys

cmd = "program %s verify reset" % sys.argv[1]

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("127.0.0.1", 6666))

sock.send(cmd.encode() + b'\x1A')

data = b''
while data [-1:] != b'\x1A':
    data += sock.recv(4096)

reply = data[:-1].decode()
if reply:
    print(reply)
