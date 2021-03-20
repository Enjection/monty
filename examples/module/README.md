This example adds a custom module to Monty, so that this script works:

```py
import demo
print('twice', 123, 'is', demo.twice(123))
```

The code for this is in `lib/my-mod/`. Sample output:

```text
$ inv
main
hello monty v1.0-175-g4585979
done
2 tests, 2 matches, 0 failures, 0 skipped, 0 ignored
Processing nucleo-l432 (board: nucleo_l432kc; platform: ststm32; framework: cmsis)
STM32 STLink: /dev/cu.usbmodem143202 ser# 066BFF555052836687031442
upload 0x000E0 done, 224 bytes sent
STM32 STLink: /dev/cu.usbmodem143202 ser# 066BFF555052836687031442
2 tests, 2 matches, 0 failures, 0 skipped, 0 ignored
$
```

In contrast to the `library` example, this is auto-installed at link time.  The
module ends up as built-in, and is immediately available to all scripts.  The
benefits is a simpler extension process, the drawback is that no custom code
can run on startup. It's just another linked-in module.
