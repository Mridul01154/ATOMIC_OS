# ATOMIC OS 🚀

A modern 32-bit x86 operating system kernel written from scratch with advanced features including graphical interface, file system, and application framework.

## 📹 Demo Video
[![ATOMIC OS Demo](https://github.com/Mridul01154/ATOMIC_OS/blob/main/Docs/Screenshot%202025-11-30%20195438.png)](https://github.com/Mridul01154/ATOMIC_OS/blob/main/Docs/Atomic%20-%20Made%20with%20Clipchamp_1764514436480.mp4)









## ✨ Features

### 🎯 Core System
- **32-bit Protected Mode** kernel with custom bootloader
- **Memory Management** with dynamic allocation
- **VGA Text Mode** display driver with advanced graphics
- **Real-time Clock** (RTC) support
- **PS/2 Keyboard** driver with full input handling

### 💾 File System
- **RAM Disk** with 1MB storage
- **File Operations**: create, read, delete, list
- **Persistent** in-memory storage

### 🖥️ User Interface
- **Advanced Text Editor** with cursor navigation
- **Command Line Interface** with 20+ commands
- **Main Menu System** with application launcher
- **Graphical Borders** and holographic displays

### 📱 Applications
- **Text Editor** with file save/load
- **Calculator** with arithmetic operations
- **File Browser** with visual management
- **System Monitor** with status information

## 🛠️ Technical Specifications

### Architecture
- **CPU**: 32-bit x86 (i686)
- **Memory**: Protected mode
- **Display**: VGA Text Mode (80x25)
- **Storage**: RAM Disk (1MB)

## 🚀 Quick Start

### Prerequisites
```bash
sudo apt-get install gcc-i686-elf nasm qemu-system-x86
```
### Building from Source
```bash
# Clone repository
git clone https://github.com/yourusername/atomic-os.git
cd atomic-os

# Build the OS
make

# Run in QEMU
make run
```
### Manual Build
```bash
# Build kernel components
make kernel

# Create bootable image
make image

# Launch in emulator
qemu-system-i386 -fda os-image.bin
```
### 📁 Project Structure
```text
atomic-os/
├── boot/                 # Bootloader assembly
│   └── boot.asm
├── kernel/              # Kernel source code
│   ├── kernel.cpp
│   ├── memory.cpp
│   └── fs_ramdisk.cpp
│   
├── include/             # Header files
│   ├── kernel.h
│   ├── memory.h
│   └── fs_ramdisk.h
│   
├── docs/               # Documentation
├── Makefile           # Build configuration
└── os-image.bin       # Final OS image
```
### 🎮 Usage Guide
## Boot Process
* System loads custom bootloader

* Enters 32-bit protected mode

* Initializes memory management

* Starts kernel main loop


## Available Applications
* Text Editor - Full-featured text editing(press esc to open)

* File Browser - Manage files in RAM disk

* System Info - View system status

* Command Line - Advanced system commands

## Command Line Reference
```bash
help        # Show all available commands
clear       # Clear terminal output
about       # Display OS information
status      # System status check
time        # Show current time
date        # Show current date
mem         # Memory usage information
meminfo     # Detailed memory statistics
mmap        # Memory map display
alloc       # Test memory allocation
ls          # List files in RAM disk
save <file> # Save file to disk
load <file> # Load file from disk
cat <file>  # Display file contents
rm <file>   # Delete File
```
## 🔧 Development
## Building Custom Components
```bash
# Add new kernel module
i686-elf-g++ -m32 -ffreestanding -c new_module.cpp -o new_module.o

# Link with existing kernel
i686-elf-ld -Ttext 0x1000 -o full_kernel.bin kernel.o new_module.o memory.o fs_ramdisk.o menu.o
```
## Debugging
```bash
# Run with QEMU debugger
qemu-system-i386 -fda os-image.bin -d cpu -D qemu.log

# Analyze boot process
make debug
```
## 📊 System Architecture
## Memory Layout
```text
0x00000000 - 0x0009FFFF: Kernel Space
0x00100000 - 0x001FFFFF: RAM Disk (1MB)
0x00200000 - 0x003FFFFF: Heap Memory
0x00400000 - 0x007FFFFF: User Space
0xB8000     - 0xB8FA0:    VGA Text Buffer
```
## File System Layout
```text
Superblock → FAT → File Table → Data Blocks
```
## 🤝 Contributing
I welcome contributions! Please see our Contributing Guide for details.

## Development Setup
* Fork the repository

* Set up cross-compiler toolchain

* Make your changes

* Test with QEMU

* Submit pull request

## 📝 License
This project is licensed under the MIT License - see the LICENSE file for details.

## 🙏 Acknowledgments
nanobyte Building an OS Tutorial

## 📞 Support
Issues: GitHub Issues

Discussions: GitHub Discussions

Email: toyloue47@gmail.com

## 🗺️ Roadmap
## Version 1.1 (Next Release)
Process management

Basic multitasking

Improved file system

Network stack foundation

## Version 2.0 (Future)
Graphical user interface

Package management

Device drivers

User authentication

### Last updated: November 2023
**__ATOMIC OS v0.1 - 32-bit Kernel with Memory Management__**
