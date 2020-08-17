#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "monty.h"
#include "arch.h"

using namespace Monty;

static auto bi_blah (Vector const& vec, int argc, int args) -> Value {
    return argc;
}

static Function const f_blah (bi_blah);

static Lookup::Item const lo_machine [] = {
    { "blah", &f_blah },
};

static Lookup const ma_machine (lo_machine, sizeof lo_machine);
extern Module const m_machine (&ma_machine);
