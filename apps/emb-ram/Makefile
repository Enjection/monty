ROOT = ../..
LIBS =
DIRS =

OPTS = -Os -fno-rtti -fno-exceptions

CXX_SOURCES =  \
	main.cpp \
	os.cpp \

PREFIX = arm-none-eabi-
CXX = $(PREFIX)g++
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
BIN = $(CP) -O binary -S

CPU = -mcpu=cortex-m7
FPU = -mfpu=fpv5-sp-d16
FLOAT-ABI = -mfloat-abi=hard
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

C_DEFS =
C_INCLUDES =
CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPTS) -Wall -fdata-sections -ffunction-sections

#CFLAGS += -g -gdwarf-2
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

LDSCRIPT = ram.ld

LDLIBS = -lc -lm -lnosys
LIBDIR =
LDFLAGS = $(MCU) --specs=nano.specs --specs=nosys.specs -T$(LDSCRIPT) $(LIBDIR) $(LDLIBS) -Wl,-Map=build/firmware.map,--cref -Wl,--gc-sections

tdd:
	@ fswatch -h >/dev/null # make sure it's installed
	@ while $(ROOT)/tools/tdd.py . $(LIBS); do :; done

all: cls main run

cls:
	@ clear && date

main: build/firmware.bin
	@ echo

run:
	@ st-flash --reset write build/firmware.bin 0x20000000 >/dev/null 2>&1

OBJECTS = $(addprefix build/,$(notdir $(CXX_SOURCES:.cpp=.o)))
vpath %.cpp $(sort $(dir $(CXX_SOURCES)))

build/%.o: %.cpp Makefile | build
	@ echo C++ $<
	@ $(CXX) -c $(CFLAGS) -Wa,-a,-ad,-alms=build/$(notdir $(<:.cpp=.lst)) $< -o $@

build/firmware.elf: $(OBJECTS) Makefile
	@ echo LD $@
	@ $(CXX) $(OBJECTS) $(LDFLAGS) -o $@
	@ $(SZ) $@

build/%.hex: build/%.elf | build
	$(CP) -O ihex $< $@

build/%.bin: build/%.elf | build
	@ $(BIN) $< $@

build:
	mkdir $@

# used by tdd.py
dirs: ; @ echo $(patsubst %,$(LIBS)/%,$(DIRS))
srcs: ; @ echo $(SRCS)

clean:
	rm -rf build

-include $(wildcard build/*.d)
