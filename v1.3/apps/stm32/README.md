An example of building Monty for a variety of STM32 boards.

The default builds for a Blue Pill:

```text
$ inv
Processing bluepill (board: bluepill_f103c8; platform: ststm32; framework: cmsis)
Black Magic Probe (SWLINK) v1.7.1-137-gf89b07d: /dev/cu.usbmodemE2C2B4DF3 ser# E2C2B4DF
1 tests, 1 matches, 0 failures, 1 skipped, 0 ignored
$
```

This can quickly be switched to a different board, e.g. `inv env circle`.

See `monty-pio.ini` for the boards which have been defined so far.
