This is the template for a new custom Monty build. Adjust as needed.

Some commands to try out are:

```text
$ inv -l
Available tasks:

  all        this is a demo "all" task, as defined in monty-inv.py
  clean      delete all build results
  env        change the default env to use in "monty-pio.ini"
  flash      build embedded and re-flash attached µC
  generate   pass source files through the code generator
  mrfs       upload tests as Minimal Replaceable File Storage image
  native     run script using the native build  [verify/hello.py]
  python     run Python tests natively          [in verify/: {*}.py]
  runner     run Python tests, sent to µC       [in verify/: {*}.py]
  serial     serial terminal session, use in separate window
  test       run C++ tests natively
  upload     run C++ tests, uploaded to µC
  watch      watch-exec/upload-print loop for quick Python iteration

Default task: all
```

For details about a specific command, such as `inv python`:

```text
$ inv -h python
Usage: inv[oke] [--core-opts] python [--options] [other tasks here ...]

Docstring:
  run Python tests natively          [in verify/: {*}.py]

Options:
  -c, --coverage              generate a code coverage report using 'kcov'
  -i, --ignore                one specific test to ignore
  -t STRING, --tests=STRING   specific tests to run, comma-separated

$
```
