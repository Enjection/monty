ROOT = ../..
LIBS =
DIRS =

OPT = -Os -fno-rtti -fno-exceptions

BUILD_DIR = build

CXX_SOURCES =  \
	main.cpp \
	os.cpp \

PREFIX = arm-none-eabi-
CXX = $(PREFIX)g++
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

CPU = -mcpu=cortex-m7
FPU = -mfpu=fpv5-sp-d16
FLOAT-ABI = -mfloat-abi=hard
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

C_DEFS =
C_INCLUDES =
CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

#CFLAGS += -g -gdwarf-2
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

LDSCRIPT = ram.ld

LDLIBS = -lc -lm -lnosys
LIBDIR =
LDFLAGS = $(MCU) --specs=nano.specs --specs=nosys.specs -T$(LDSCRIPT) $(LIBDIR) $(LDLIBS) -Wl,-Map=$(BUILD_DIR)/firmware.map,--cref -Wl,--gc-sections

tdd:
	@ fswatch -h >/dev/null # make sure it's installed
	@ while $(ROOT)/tools/tdd.py . $(LIBS); do :; done

all: cls main run

cls:
	@ clear && date
main: $(BUILD_DIR)/firmware.elf
	@ $(BIN) $(BUILD_DIR)/firmware.elf $(BUILD_DIR)/firmware.bin
run:
	@ st-flash --reset write $(BUILD_DIR)/firmware.bin 0x20000000 >/dev/null 2>&1

OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(CXX_SOURCES:.cpp=.o)))
vpath %.cpp $(sort $(dir $(CXX_SOURCES)))

$(BUILD_DIR)/%.o: %.cpp Makefile | $(BUILD_DIR)
	@ echo C++ $<
	@ $(CXX) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.cpp=.lst)) $< -o $@

$(BUILD_DIR)/firmware.elf: $(OBJECTS) Makefile
	@ echo LD $@
	@ $(CXX) $(OBJECTS) $(LDFLAGS) -o $@
	@ $(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	@ $(HEX) $< $@
	@ wc -c $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	@ $(BIN) $< $@
	@ wc -c $@

$(BUILD_DIR):
	mkdir $@

# used by tdd.py
dirs: ; @ echo $(patsubst %,$(LIBS)/%,$(DIRS))
srcs: ; @ echo $(SRCS)

clean:
	rm -rf $(BUILD_DIR)

-include $(wildcard $(BUILD_DIR)/*.d)
