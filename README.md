## Monty: a stackless VM

This is a virtual machine which executes bytecode generated by [MicroPython][MPY].  
It's grossly incomplete and totally unfit for general use, but it _does_ work.  
There is no compiler, **Monty** requires an `.mpy` file from `mpy-cross` to run.  

This C++ library is written from the ground up, but it would not exist without  
the huge amount of thought and work put into the development of MicroPython,  
which proves that a modern dynamic language can run well on embedded µCs.

The reason for creating this project, is to explore some stackless design ideas.

See `apps/native/` for details - **the directory structure is still in flux.**

[MPY]: https://micropython.org/
