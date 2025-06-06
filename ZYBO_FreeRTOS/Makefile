################################################################################
# FreeRTOS UART Interrupt Project - Zybo Board
# Toolchain: arm-none-eabi-gcc
################################################################################

# Project settings
TARGET = freertos_sample_irq
BUILD_DIR = build
SRC_DIR = src
INCLUDE_DIR = $(SRC_DIR)/include

# Toolchain and flags
CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
SIZE = arm-none-eabi-size
CFLAGS = -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard -O2 -I$(INCLUDE_DIR)
LDFLAGS = -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard -Wl,-build-id=none -specs=Xilinx.spec -Wl,-T,$(SRC_DIR)/lscript.ld -Llib $(LIBS)
LIBS = -Wl,--start-group,-lxil,-lfreertos,-lgcc,-lc,--end-group

# Source and object files
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))

# Default target
all: $(TARGET).elf

# Linking
$(TARGET).elf: $(OBJS)
	@echo "Building target: $@"
	$(LD) -o $@ $^ $(LDFLAGS)
	@$(SIZE) $@

# Compilation
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(SRC_DIR)/*.o $(TARGET).elf $(TARGET).elf.size

.PHONY: all clean

