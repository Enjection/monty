namespace monty {
    auto vmImport (char const* name) -> uint8_t const*;
    auto vmLaunch (void const* data) -> Context*;
}
