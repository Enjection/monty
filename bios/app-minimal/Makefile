CFLAGS = -Os -fno-rtti -fno-exceptions

CXX_SOURCES =  \
	main.cpp \

PREFIX = arm-none-eabi-
CC = $(PREFIX)g++
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size

CPU = -mcpu=cortex-m7
FPU = -mfpu=fpv5-sp-d16
FLOAT-ABI = -mfloat-abi=hard
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

C_DEFS =
C_INCLUDES = -I../os-jeeh/src

CFLAGS += $(MCU) $(C_DEFS) $(C_INCLUDES)
CFLAGS += -Wall -fdata-sections -ffunction-sections
#CFLAGS += -g -gdwarf-2
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

LIBS = -lc -lm -lnosys
LIBDIR =
LDFLAGS = $(MCU) --specs=nano.specs --specs=nosys.specs -Tram.ld $(LIBDIR) $(LIBS) -Wl,--gc-sections

all: build/firmware.bin

OBJECTS = $(addprefix build/,$(notdir $(CXX_SOURCES:.cpp=.o)))
vpath %.cpp $(sort $(dir $(CXX_SOURCES)))

build/%.o: %.cpp Makefile | build
	@ echo c++ $<
	@ $(CC) -c $(CFLAGS) $< -o $@

build/firmware.elf: $(OBJECTS) Makefile
	@ echo ld $@
	@ $(CC) $(OBJECTS) $(LDFLAGS) -o $@
	@ $(SZ) $@

build/%.bin: build/%.elf | build
	@ $(CP) -O binary -S $< $@

build:
	mkdir $@

upload: all
	st-flash --reset write build/firmware.bin 0x20001000 >/dev/null 2>&1

clean:
	@ rm -rf build

-include $(wildcard build/*.d)
