import graphics as g

Black   = const(0x0000)
Red     = const(0xF800)
Green   = const(0x07E0)
Blue    = const(0x001F)
Yellow  = const(0xFFE0)
Cyan    = const(0x07FF)
Magenta = const(0xF81F)
White   = const(0xFFFF)

g.init()
g.clear()

for y in range(10, 20):
    for x in range(10, 60):
        g.pixel(x, y)

g.hline(5, 2, 230)
g.vline(5, 5, 200)

g.line(230, 220, 10, 220)
g.line(230, 220, 230, 20)

g.box(80, 40, 105, 65)
g.fg(Magenta)
g.box(150, 40, 120, 70)

g.fg(Red)
g.bfill(160, 40, 185, 65)
g.fg(Green)
g.bfill(190, 70, 220, 40)

g.fg(Yellow)
g.line(100, 100, 150, 125)
g.line(150, 125, 100, 150)
g.line(100, 150, 125, 100)
g.line(125, 100, 140, 150)
g.line(140, 150, 100, 100)

g.fg(Cyan)
g.triangle(30, 30, 70, 60, 60, 80)
g.fg(White)
g.tfill(30, 80, 70, 110, 60, 130)

g.fg(Red)
g.dashed(200, 30, 200, 80, 0x27272727)
g.bg(Red)
g.dashed(210, 30, 210, 80, 0x27272727)

g.fg(Yellow)
g.bg(Yellow)
g.dashed(160, 40, 184, 64, 0x0F0F0F0F)

g.fg(White)
g.bg(Black)

g.circle(50, 175, 20)
g.cfill(55, 180, 10)

g.round(180, 130, 210, 170, 10)
g.rfill(160, 180, 220, 200, 5)

g.fg(Yellow)
g.writes(80, 12, "Hello, Monty!")
