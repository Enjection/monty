This is a set of low-level "smoke" tests for the build, board, and basics.  
There is no dependency on anything other than PyInvoke and PlatformIO.  
No Monty headers, libraries, or scripts are used here, only Hall & Boss.

Things to try:

```text
inv -l
inv test01
inv test02
inv test03
[etc]
```

For `test02` and up, a Nucleo-L432KC board needs to be plugged into USB.

## Test 10

This test uses 6 additional LEDs, connected to the Nucleo's A0..A5:

![](image.jpg)

A6 is tied to GND, and each LED has a current-limiting resistor in series.
