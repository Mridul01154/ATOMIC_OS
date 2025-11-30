# Compiler / Tools
CC = i686-elf-gcc
CXX = i686-elf-g++
LD = i686-elf-ld
ASM = nasm
OBJCOPY = i686-elf-objcopy

# Flags
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -c -g
CXXFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -c -g -fno-rtti -fno-exceptions
ASMFLAGS_BIN = -f bin
ASMFLAGS_ELF = -f elf
LDFLAGS = -Ttext 0x1000 --oformat binary

# Sources
BOOT_SRC = boot.asm
KERNEL_SRC = kernel.cpp
MEMORY_SRC = memory.cpp
FS_RAMDISK_SRC = fs_ramdisk.cpp
KERNEL_ENTRY_SRC = kernel_entry.asm
ZEROES_SRC = zeroes.asm

# Objects
BOOT_BIN = boot.bin
KERNEL_OBJ = kernel.o
MEMORY_OBJ = memory.o
FS_RAMDISK_OBJ = fs_ramdisk.o
KERNEL_ENTRY_OBJ = kernel_entry.o
FULL_KERNEL_BIN = full_kernel.bin
ZEROES_BIN = zeroes.bin
EVERYTHING_BIN = everything.bin
OS_BIN = OS.bin

# Headers (for dependency tracking)
HEADERS = memory.h fs_ramdisk.h

# Default target
all: $(OS_BIN)

# Build final OS image
$(OS_BIN): $(EVERYTHING_BIN) $(ZEROES_BIN)
	cat $(EVERYTHING_BIN) $(ZEROES_BIN) > $(OS_BIN)

# Combine bootloader + kernel binary
$(EVERYTHING_BIN): $(BOOT_BIN) $(FULL_KERNEL_BIN)
	cat $(BOOT_BIN) $(FULL_KERNEL_BIN) > $(EVERYTHING_BIN)

# Compile bootloader to binary
$(BOOT_BIN): $(BOOT_SRC)
	$(ASM) $(ASMFLAGS_BIN) $(BOOT_SRC) -o $(BOOT_BIN)

# Link kernel entry + kernel C++ + memory + file system
$(FULL_KERNEL_BIN): $(KERNEL_ENTRY_OBJ) $(KERNEL_OBJ) $(MEMORY_OBJ) $(FS_RAMDISK_OBJ)
	$(LD) $(LDFLAGS) -o $(FULL_KERNEL_BIN) $(KERNEL_ENTRY_OBJ) $(KERNEL_OBJ) $(MEMORY_OBJ) $(FS_RAMDISK_OBJ)

# Compile kernel C++ code (depends on headers)
$(KERNEL_OBJ): $(KERNEL_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(KERNEL_SRC) -o $(KERNEL_OBJ)

# Compile memory C++ code
$(MEMORY_OBJ): $(MEMORY_SRC) memory.h
	$(CXX) $(CXXFLAGS) $(MEMORY_SRC) -o $(MEMORY_OBJ)

# Compile RAM disk file system
$(FS_RAMDISK_OBJ): $(FS_RAMDISK_SRC) fs_ramdisk.h memory.h
	$(CXX) $(CXXFLAGS) $(FS_RAMDISK_SRC) -o $(FS_RAMDISK_OBJ)

# Assemble kernel entry assembly
$(KERNEL_ENTRY_OBJ): $(KERNEL_ENTRY_SRC)
	$(ASM) $(ASMFLAGS_ELF) $(KERNEL_ENTRY_SRC) -o $(KERNEL_ENTRY_OBJ)

# Compile zeroes binary
$(ZEROES_BIN): $(ZEROES_SRC)
	$(ASM) $(ASMFLAGS_BIN) $(ZEROES_SRC) -o $(ZEROES_BIN)

# Clean all build files
clean:
	rm -f $(KERNEL_OBJ) $(MEMORY_OBJ) $(FS_RAMDISK_OBJ) $(KERNEL_ENTRY_OBJ)
	rm -f $(BOOT_BIN) $(FULL_KERNEL_BIN) $(EVERYTHING_BIN) $(ZEROES_BIN) $(OS_BIN)

# Run in QEMU
run: $(OS_BIN)
	qemu-system-i386 -fda $(OS_BIN)

# Debug build with extra symbols
debug: CXXFLAGS += -DDEBUG -Og
debug: all

# Build only the file system module
fs_only: $(FS_RAMDISK_OBJ)
	@echo "File system module built: $(FS_RAMDISK_OBJ)"

# Show file sizes
size: $(FULL_KERNEL_BIN)
	@echo "Kernel binary size:"
	@stat -c%s $(FULL_KERNEL_BIN) || wc -c < $(FULL_KERNEL_BIN)
	@echo "Full OS image size:"
	@stat -c%s $(OS_BIN) || wc -c < $(OS_BIN)

# Show help
help:
	@echo "Available targets:"
	@echo "  all      - Build the complete OS image (default)"
	@echo "  clean    - Remove all build files"
	@echo "  run      - Run the OS in QEMU"
	@echo "  debug    - Build with debug symbols"
	@echo "  fs_only  - Build only the file system module"
	@echo "  size     - Show binary sizes"
	@echo "  help     - Show this help message"

.PHONY: all clean run debug fs_only size help