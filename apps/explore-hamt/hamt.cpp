#include <cstdint>
#include <cstring>

#include "objs.h"

using namespace monty;

struct Hamt {
    auto size () const { return 0; }
};

#if DOCTEST
#include <doctest.h>
namespace {

TEST_CASE("make root") {
    uint8_t memory [3*1024];
    objInit(memory, sizeof memory);

    Hamt root;

    CHECK(root.size() == 0);
}

}
#endif
