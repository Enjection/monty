DocTest is a C++ testing framework by Viktor Kirilov (MIT license), see:

* <https://github.com/onqtam/doctest>
* <https://github.com/onqtam/doctest/tree/master/doc/markdown>

Monty uses this for testing non-embedded builds, with tests added at the end
of the source code, using this convention:

```
#if DOCTEST
#include <doctest.h>

TEST_CASE(...) {
    ...
}

#endif // DOCTEST
```

By keeping these tests as part of the actual source files, they can access any
variables and functions defined statically, i.e. not mentioned in the headers.

For a TDD workflow which runs these tests, see `../../tests/fiber/`.
