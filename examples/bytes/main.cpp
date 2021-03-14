#include <monty.h>
#include <arch.h>

using namespace monty;

#define SIZEOF(name) printf("%5d b  %s\n", (int) sizeof (name), #name);

int main () {
    arch::init(1024);
#if !NATIVE
    for (int i = 0; i < 50000000; ++i) asm (""); // brief delay
#endif

    SIZEOF(Array)
    SIZEOF(Bool)
    SIZEOF(Buffer)
    SIZEOF(Bytes)
    SIZEOF(Class)
    SIZEOF(Dict)
    SIZEOF(Event)
    SIZEOF(Exception)
    SIZEOF(Inst)
    SIZEOF(Int)
    SIZEOF(Iterator)
    SIZEOF(List)
    SIZEOF(Lookup)
    SIZEOF(Lookup::Item)
    SIZEOF(Method)
    SIZEOF(Module)
    SIZEOF(None)
    SIZEOF(Object)
    SIZEOF(Range)
    SIZEOF(Set)
    SIZEOF(Slice)
    SIZEOF(Stacklet)
    SIZEOF(Str)
    SIZEOF(Super)
    SIZEOF(Tuple)
    SIZEOF(Type)
    SIZEOF(Value)
    SIZEOF(VaryVec)
    SIZEOF(Vec)
    SIZEOF(Vector)

    return 0; // don't use arch::done, which will reset - just stop
}
