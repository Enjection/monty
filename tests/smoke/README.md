This is a set of low-level "smoke" tests for the build, board, and basics.  
There is no dependency on anything other than PyInvoke and PlatformIO.  
No Monty headers, libraries, or scripts are used in the first few tests.

Things to try:

```text
inv -l
inv test1
inv test2
inv test3
[etc]
```

Test2 and up require a Nucleo-L432KC board to be connected via USB.
