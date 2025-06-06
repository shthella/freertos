#
# Copyright (c) 2012-2021 Andes Technology Corporation
# All rights reserved.
#

PROG	?= Demo

ifndef CROSS_COMPILE
	ifneq (,$(filter $(COMPILER),riscv32 riscv32-llvm))
		CROSS_COMPILE = riscv32-elf-
	endif
endif
ifndef CROSS_COMPILE
	ifneq (,$(filter $(COMPILER),riscv64 riscv64-llvm))
		CROSS_COMPILE = riscv64-elf-
	endif
endif
ifndef CROSS_COMPILE
$(error You have to specify 'COMPILER' to build ! Ex: make COMPILER=[riscv32|riscv64|riscv32-llvm|riscv64-llvm])
endif

ifneq (,$(filter $(COMPILER),riscv32-llvm riscv64-llvm))
CC	:= $(CROSS_COMPILE)clang
C++	:= $(CROSS_COMPILE)clang++
else
CC	:= $(CROSS_COMPILE)gcc
C++     := $(CROSS_COMPILE)g++
endif
OBJDUMP	:= $(CROSS_COMPILE)objdump
OBJCOPY := $(CROSS_COMPILE)objcopy
AR	:= $(CROSS_COMPILE)ar
AS	:= $(CROSS_COMPILE)as

ifeq ($(DEBUG),1)
	OPTIM   := -O0 -g3
else
	OPTIM   := -Os -g3
endif

# Platform: AE250, CF1-AE250
PLAT	?= AE250

# Mode: LOAD, BURN, XIP
MODE	?= LOAD

ifneq ($(filter $(PLAT), AE250 CF1-AE250),$(PLAT))
$(error Unknown PLAT "$(PLAT)" is not supported!)
endif

# Define BOARD
ifeq ($(PLAT),CF1-AE250)
	BOARD	:= CF1
else
	BOARD	:= ORCA
endif

PLATNAME = $(shell echo $(patsubst CF1-%,%,$(PLAT)) | tr A-Z a-z)

# Define the directories
FREERTOS_SOURCE_DIR	= ../../Source
APP_SOURCE_DIR		= ../Common/Minimal
BSP_SOURCE_DIR		= ./RTOSDemo_bsp
DEMO_SOURCE_DIR		= ./RTOSDemo

# Define the source files we have

# BSP source files
STARTUP_SRCS = $(BSP_SOURCE_DIR)/start.S

BSP_SRCS = \
	$(BSP_SOURCE_DIR)/reset.c \
	$(BSP_SOURCE_DIR)/initfini.c \
	$(BSP_SOURCE_DIR)/$(PLATNAME)/$(PLATNAME).c \
	$(BSP_SOURCE_DIR)/$(PLATNAME)/interrupt.c \
	$(BSP_SOURCE_DIR)/$(PLATNAME)/iochar.c \
	$(BSP_SOURCE_DIR)/driver/uart.c

ifeq ($(MODE), BURN)
	BSP_SRCS += $(BSP_SOURCE_DIR)/$(PLATNAME)/loader.c
endif

LIBC_SRCS = \
	$(BSP_SOURCE_DIR)/libc/stdio/do_printf.c \
	$(BSP_SOURCE_DIR)/libc/stdio/printf.c

# Kernel source files
FREERTOS_SRCS = \
	$(FREERTOS_SOURCE_DIR)/croutine.c \
	$(FREERTOS_SOURCE_DIR)/list.c \
	$(FREERTOS_SOURCE_DIR)/queue.c \
	$(FREERTOS_SOURCE_DIR)/tasks.c \
	$(FREERTOS_SOURCE_DIR)/timers.c \
	$(FREERTOS_SOURCE_DIR)/event_groups.c \
	$(FREERTOS_SOURCE_DIR)/stream_buffer.c \
	$(FREERTOS_SOURCE_DIR)/portable/MemMang/heap_2.c

PORT_SRCS = \
	$(FREERTOS_SOURCE_DIR)/portable/GCC/RISC-V/port.c \
	$(FREERTOS_SOURCE_DIR)/portable/GCC/RISC-V/portASM.S

RTOS_SRCS = $(FREERTOS_SRCS) $(PORT_SRCS)

# Demo source files
APP_SRCS = \
	$(APP_SOURCE_DIR)/AbortDelay.c \
	$(APP_SOURCE_DIR)/BlockQ.c \
	$(APP_SOURCE_DIR)/blocktim.c \
	$(APP_SOURCE_DIR)/countsem.c \
	$(APP_SOURCE_DIR)/death.c \
	$(APP_SOURCE_DIR)/dynamic.c \
	$(APP_SOURCE_DIR)/EventGroupsDemo.c \
	$(APP_SOURCE_DIR)/flop.c \
	$(APP_SOURCE_DIR)/GenQTest.c \
	$(APP_SOURCE_DIR)/IntSemTest.c \
	$(APP_SOURCE_DIR)/QueueOverwrite.c \
	$(APP_SOURCE_DIR)/recmutex.c \
	$(APP_SOURCE_DIR)/semtest.c \
	$(APP_SOURCE_DIR)/StaticAllocation.c \
	$(APP_SOURCE_DIR)/TaskNotify.c \
	$(APP_SOURCE_DIR)/TimerDemo.c

DEMO_SRCS = \
	$(DEMO_SOURCE_DIR)/main.c \
	$(DEMO_SOURCE_DIR)/Blinky_Demo/main_blinky.c \
	$(DEMO_SOURCE_DIR)/Full_Demo/main_full.c \
	$(DEMO_SOURCE_DIR)/Full_Demo/reg_test.S \
	$(DEMO_SOURCE_DIR)/FreeRTOS_tick_config.c

# Define all object files.
SRCS = $(STARTUP_SRCS) $(BSP_SRCS) $(LIBC_SRCS) $(RTOS_SRCS) $(APP_SRCS) $(DEMO_SRCS)
OBJS = $(patsubst %.S,%.o,$(patsubst %.cpp,%.o,$(patsubst %.c,%.o,${SRCS})))

### Verbosity control. Use 'make V=1' to get verbose builds.

ifeq ($(V),1)
TRACE_CC  =
TRACE_C++ =
TRACE_LD  =
TRACE_AR  =
TRACE_AS  =
Q=
else
TRACE_CC  = @echo "  CC       " $<
TRACE_C++ = @echo "  C++      " $<
TRACE_LD  = @echo "  LD       " $@
TRACE_AR  = @echo "  AR       " $@
TRACE_AS  = @echo "  AS       " $<
Q=@
endif

# Compilation options

LDSCRIPT = $(BSP_SOURCE_DIR)/$(PLATNAME)/$(shell echo $(PLAT) | tr A-Z a-z).ld

CMODEL = -mcmodel=medany

#
# FreeRTOS needs to define an "portasmHANDLE_INTERRUPT" assembler macro to tell
# the FreeRTOS kernel which external interrupt handler to call.
# Besides, it also needs to specify the linker variable called "__freertos_irq_stack_top"
# that holds the highest address of the interrupt stack. We set __freertos_irq_stack_top
# to equal the value of linker variable "_stack".
#
DEFINES = -DCFG_$(BOARD) -DCFG_$(MODE) -DportasmHANDLE_INTERRUPT=clic_interrupt -D__freertos_irq_stack_top=_stack

INCLUDES = \
	-I. \
	-I../Common/include \
	-I$(APP_SOURCE_DIR) \
	-I$(DEMO_SOURCE_DIR) \
	-I$(BSP_SOURCE_DIR) \
	-I$(BSP_SOURCE_DIR)/$(PLATNAME) \
	-I$(BSP_SOURCE_DIR)/driver/include \
	-I$(FREERTOS_SOURCE_DIR)/include \
	-I$(FREERTOS_SOURCE_DIR)/portable/GCC/RISC-V \
	-I$(FREERTOS_SOURCE_DIR)/portable/GCC/RISC-V/chip_specific_extensions/Andes_V5_RV32RV64

CFLAGS = \
	-Wall $(OPTIM) $(INCLUDES) $(DEFINES) \
	-fomit-frame-pointer -fno-strict-aliasing -fno-builtin -funroll-loops \
	-ffunction-sections -fdata-sections \
	$(CMODEL)

CXXFLAGS = $(CFLAGS)

ASFLAGS = -D__ASSEMBLY__ $(CFLAGS) -c

LDFLAGS = -T$(LDSCRIPT) $(OPTIM) -static -nostartfiles -Wl,--gc-sections,--defsym=__rtos_signature_freertos_v10_2_1=0x0 \
	$(CMODEL)

### Make variables

ifeq ($(USE_CACHE), 1)
	CFLAGS += -DCFG_CACHE_ENABLE
endif

# Compilation rules
.SUFFIXES : %.o %.c %.cpp %.S

%.o: %.c
	$(TRACE_CC)
	$(Q)$(CC) -c -MMD $(CFLAGS) -o $@ $<

%.o: %.cpp
	$(TRACE_C++)
	$(Q)$(C++) -c -MMD $(CXXFLAGS) -o $@ $<

%.o: %.S
	$(TRACE_CC)
	$(Q)$(CC) -c -MMD $(CFLAGS) -o $@ $<

all: $(PROG).elf

$(PROG).elf  : $(OBJS) Makefile
	$(TRACE_LD)
	$(Q)$(CC) -o $@ $(LDFLAGS) $(OBJS) $(LIBS)
	$(OBJCOPY) -O binary -S $(PROG).elf $(PROG).bin
	$(OBJDUMP) -d $(PROG).elf > $(PROG).asm
	@echo Completed

clean :
	@rm -f $(OBJS) $(OBJS:.o=.d)
	@rm -f $(PROG).elf
	@rm -f $(PROG).bin
	@rm -f $(PROG).asm

# Automatic dependency generation
ifneq ($(MAKECMDGOALS),clean)
-include $(OBJS:.o=.d)
endif


