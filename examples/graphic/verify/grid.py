import graphics as g

depth = g.init()

if depth == 16:
    Black   = 0x0000
    Red     = 0xF800
    Green   = 0x07E0
    Yellow  = 0xFFE0
    Blue    = 0x001F
    Magenta = 0xF81F
    Cyan    = 0x07FF
    White   = 0xFFFF
else:
    Black   = 0
    Red     = 1
    Green   = 2
    Yellow  = 3
    Blue    = 4
    Magenta = 5
    Cyan    = 6
    White   = 7

g.clear()

x0, y0 = 20, 20

g.fg(0x7BEF)
g.box(x0, y0, x0+199, y0+199)

for y in range(20, 200, 20):
    g.dashed(x0, y0+y, x0+200, y0+y)
for x in range(20, 200, 20):
    g.dashed(x0+x, y0, x0+x, y0+200)
