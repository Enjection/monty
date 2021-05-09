This is a set of low-level "smoke" tests for the build, board, and basics.  
There is no dependency on anything other than PyInvoke and PlatformIO.  
No Monty headers, libraries, or scripts are used here, only Hall & Boss.

Things to try:

```text
inv -l
inv test1
inv test2
inv test3
[etc]
```

For `test2` and up, a Nucleo-L432KC board needs to be plugged into USB.
