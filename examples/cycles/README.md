Get some cycle counts from an embedded build.  Sample run:
```text
$ inv
Processing nucleo-l432 (board: nucleo_l432kc; platform: ststm32; framework: cmsis)
STM32 STLink: /dev/cu.usbmodem143202 ser# 066BFF555052836687031442
--------------------------- verify/lookup.py --------------------------- out: 11
main
cycles 2150 204 599

pslot 11928 1113 6168 5177

pslot 14890 980 6300 5309

print 14525 1965 7286 5176

print 14873 1965 7283 5303
--------------------------- verify/send.py ------------------------------ out: 5
main
cycles [2057,204,599,1868]
nooper [28811,3259,3158,2868,3158,2744,3296,2744,3286,2744]
sender [49587,7874,8056,7496,8295,7509,8498,7488,7892,7945]
done
3 tests, 1 matches, 0 failures, 0 skipped, 0 ignored
```

This expects a Nucleo-L432KC board to be connected.
