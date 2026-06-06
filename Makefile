# Toolchain definition
CROSS_COMPILE ?= aarch64-none-elf-
CC      = $(CROSS_COMPILE)gcc
AS      = $(CROSS_COMPILE)as
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
STRIP   = $(CROSS_COMPILE)strip

IMG_NAME ?= Image

# Project structure
BUILD_DIR ?= build
SRC_DIR   = src

INCLUDES = -I$(SRC_DIR)/include

LINKER_LDS_SCRIPT = $(SRC_DIR)/include/linker/linker.lds
LINKER_LD_SCRIPT = $(LINKER_LDS_SCRIPT:%.lds=$(BUILD_DIR)/linker.ld)
LINKER_PRE_PROCESSOR_FLAGS = -E -P -x c

# Flags
# -Wall -Wextra: Enable all warnings
# -ffreestanding: No standard library environment
# -nostdlib: Don't link against system libraries
# -mgeneral-regs-only: Restrict to general-purpose registers (AArch64) needed for variadic functions
#						Without this, the printf implementation in kprintf.c fails.
# 						Todo: Find root cause of this and remove this flag if possible.
CFLAGS  = -c -Wall -Wextra -ffreestanding -nostdlib -mgeneral-regs-only
ASFLAGS = -c -x assembler-with-cpp
LDFLAGS = -T $(LINKER_LD_SCRIPT)

DEBUG_FLAGS = -g
RELEASE_FLAGS = -O3

# build type: debug or release (default: release)
BUILD_TYPE = release
ifeq ($(DEBUG),1)
  BUILD_TYPE = debug
else
  BUILD_TYPE = release
endif

# Output binary name
TARGET_ELF = $(BUILD_DIR)/images/$(IMG_NAME).elf
TARGET = $(BUILD_DIR)/images/$(IMG_NAME)

# Source files
SRCS_C  = $(SRC_DIR)/kernel/allocator/page_allocator.c \
		  $(SRC_DIR)/kernel/exit/exit.c \
		  $(SRC_DIR)/kernel/main.c \
		  $(SRC_DIR)/kernel/drivers/uart.c \
		  $(SRC_DIR)/kernel/error/panic.c \
		  $(SRC_DIR)/kernel/error/error_strings.c \
		  $(SRC_DIR)/kernel/exception_handling/handler.c \
		  $(SRC_DIR)/kernel/fdt/fdt.c \
		  $(SRC_DIR)/kernel/mmu/mmu.c \
		  $(SRC_DIR)/kernel/page_table/page_table.c \
		  $(SRC_DIR)/kernel/utils/kprintf.c \
		  $(SRC_DIR)/kernel/utils/string.c \
		  $(SRC_DIR)/kernel/utils/utils.c

SRCS_AS = $(SRC_DIR)/boot/boot.s \
		  $(SRC_DIR)/kernel/exception_handling/vector.s

# only used for formatting and linting
SRCS_H  = $(shell find $(SRC_DIR) -name '*.h')

OBJS_C  = $(SRCS_C:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJS_AS = $(SRCS_AS:$(SRC_DIR)/%.s=$(BUILD_DIR)/%.o)
OBJS    = $(OBJS_C) $(OBJS_AS)

# --- Verbosity Control ---
# If V is not 1, we prefix commands with @ to hide them.
ifeq ($(V),1)
  VERBOSE_PREFIX :=
else
  VERBOSE_PREFIX := @
endif

ifeq ($(BUILD_TYPE),debug)
  CFLAGS += $(DEBUG_FLAGS)
else
  CFLAGS += $(RELEASE_FLAGS)
  POST_BUILD = $(VERBOSE_PREFIX)$(STRIP) --strip-all $<
endif

ifeq ($(TEST),1)
  CFLAGS += -DRUN_TESTS
  TEST_SRCS = $(shell find tests/ -name '*.c')
  TEST_OBJS = $(TEST_SRCS:tests/%.c=$(BUILD_DIR)/tests/%.o)
  OBJS += $(TEST_OBJS)
endif

# --- libfdt ---
LIBFDT_DIR = ./external/dtc/libfdt
INCLUDES   += "-I$(LIBFDT_DIR)"


LIBFDT_OBJS := fdt.o fdt_ro.o fdt_wip.o fdt_sw.o fdt_rw.o \
               fdt_strerror.o fdt_empty_tree.o fdt_addresses.o \
               fdt_overlay.o
LIBFDT_TARGETS := $(addprefix $(BUILD_DIR)/libfdt/, $(LIBFDT_OBJS))

.PHONY: all \
		submodules \
		clean \
		docs \
		clean-docs \
		format \
		clang-tidy \
		clang-tidy-fix \
		clean-subdirs \
		tools/register_decoder

all: $(TARGET) tools/register_decoder

submodules:
	@echo "Ensuring git submodules are initialized..."
	$(VERBOSE_PREFIX)git submodule update --init --recursive

# Convert ELF to raw Binary
$(TARGET): $(TARGET_ELF)
	@mkdir -p $(dir $@)
	$(POST_BUILD)
	@echo "OBJCOPY $@"
	$(VERBOSE_PREFIX)$(OBJCOPY) -O binary $< $@

# Compile C files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC  $<"
	$(VERBOSE_PREFIX)$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compile assembly files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p $(dir $@)
	@echo "CC  $<"
	$(VERBOSE_PREFIX)$(CC) $(ASFLAGS) $(INCLUDES) $< -o $@

# Preprocess the linker script to resolve symbols and generate
# the final linker script used for linking
$(LINKER_LD_SCRIPT): $(LINKER_LDS_SCRIPT)
	@mkdir -p $(dir $@)
	@echo "Processing linker script..."
	$(VERBOSE_PREFIX)$(CC) $(LINKER_PRE_PROCESSOR_FLAGS) $(INCLUDES) $< -o $@

# Link the kernel
$(TARGET_ELF): $(OBJS) $(LIBFDT_TARGETS) $(LINKER_LD_SCRIPT)
	@mkdir -p $(dir $@)
	@echo "LD  $@"
	$(VERBOSE_PREFIX)$(LD) $(LDFLAGS) $(OBJS) $(LIBFDT_TARGETS) -o $@

$(BUILD_DIR)/libfdt/%.o: $(LIBFDT_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

run: $(TARGET)
	@echo "Running QEMU..."
	$(VERBOSE_PREFIX)qemu-system-aarch64 -machine virt \
	-cpu cortex-a57 -nographic -kernel $(TARGET) -no-reboot \
	-semihosting

format: $(SRCS_C) $(SRCS_H) $(TEST_SRCS)
	@echo "Formatting source files..."
	$(VERBOSE_PREFIX)clang-format -i $^
	$(VERBOSE_PREFIX)$(MAKE) -C tools/register_decoder  BUILD_DIR=../../$(BUILD_DIR) format

clang-tidy: $(SRCS_C) $(TEST_SRCS)
	@echo "Running clang-tidy..."
	$(VERBOSE_PREFIX)clang-tidy $^ -- $(CFLAGS) $(INCLUDES) --target=aarch64-linux-gnu

clang-tidy-fix: $(SRCS_C) $(TEST_SRCS)
	@echo "Running clang-tidy..."
	$(VERBOSE_PREFIX)clang-tidy $^ --fix -- $(CFLAGS) $(INCLUDES) --target=aarch64-linux-gnu

docs:
	@echo "Generating Doxygen documentation..."
	@mkdir -p build/docs
	@doxygen docs/Doxyfile
	@echo "Documentation generated at build/docs/html/index.html"

clean-docs:
	@echo "Cleaning documentation..."
	@rm -rf build/docs

tools/register_decoder:
	$(VERBOSE_PREFIX)$(MAKE) -C $@  BUILD_DIR=../../$(BUILD_DIR)

$(BUILD_DIR)/tests/%.o: tests/%.c
	@mkdir -p $(dir $@)
	@echo "CC  $<"
	$(VERBOSE_PREFIX)$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean: clean-docs
	@echo "Cleaning build directory..."
	$(VERBOSE_PREFIX)rm -rf $(BUILD_DIR) $(TARGET) $(TARGET_ELF)
