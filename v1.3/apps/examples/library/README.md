This examples adds a custom library to Monty, so that this script works:

```py
print('triple', 333, 'is', triple(333))
print('quadruple', 222, 'is', int.quadruple(222))
print('quadruple', 111, 'is', (111).quadruple())
```

The code for this is in `lib/my-lib/`. Sample output:

```text
$ inv
main
hello monty v1.0-175-g4585979
done
2 tests, 2 matches, 0 failures, 0 skipped, 0 ignored
Processing nucleo-l432 (board: nucleo_l432kc; platform: ststm32; framework: cmsis)
STM32 STLink: /dev/cu.usbmodem143202 ser# 066BFF555052836687031442
upload 0x00120 done, 288 bytes sent
STM32 STLink: /dev/cu.usbmodem143202 ser# 066BFF555052836687031442
2 tests, 2 matches, 0 failures, 0 skipped, 0 ignored
$
```

In contrast to the `module` example, this does not auto-install itself as
built-in. Instead, a custom `main.cpp` is used to install all new code. The
benefit is more control, the drawback is th need for a custom `main.cpp`.
