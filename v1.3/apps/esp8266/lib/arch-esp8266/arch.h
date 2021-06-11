//CG: includes

namespace arch {
    auto cliTask () -> monty::Stacklet*;

    void init (int =0);
    void idle ();
    void done [[noreturn]] ();
}

#define MAIN extern "C" void user_init ()
