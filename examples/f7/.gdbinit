tar ext | openocd -f board/stm32f7discovery.cfg -c "gdb_port pipe"
mon halt
mon tpiu config internal :6464 uart false 200000000 115200
file .pio/build/disco-f750/firmware.elf
load
run
