This area has low-level tests to make sure the build & board work.  
There is no dependency on anything other than PyInvoke and PlatformIO.  
No Monty headers, libraries, or scripts are used in the first few tests.

Things to try:

```text
inv
inv -l
inv smoke1
inv smoke2
inv smoke3
[etc...]
```

Smoke2 and up require a Nucleo-L432KC board to be connected via USB.
