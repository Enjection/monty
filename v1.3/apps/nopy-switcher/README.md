This has been configured to run on a [Nucleo-L432KC][L432].

To build and run (this echos the underlying cmd used to do the work):

```text
Processing nucleo-l432 (board: nucleo_l432kc; platform: ststm32; framework: cmsis)
--- Available filters and text transformations: colorize, debug, default, direct, hexlify, log2file, nocontrol, printable, send_on_enter, time
--- More details at http://bit.ly/pio-monitor-filters
--- Miniterm on /dev/cu.usbmodem143202  921600,8,N,1 ---
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
659540 0
�
659540 0
�
659540 0
^C
```

Thsi reports the number of cycles needed for 1000 stacklet switches.
The demo repeats indefinitely, because it restarts after every run.

[L432]: https://www.st.com/en/evaluation-tools/nucleo-l432kc.html
