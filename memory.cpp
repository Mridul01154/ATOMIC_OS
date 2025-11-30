#include "memory.h"

// Define the global instances
SimpleAllocator g_allocator;

// Global memory map
MemoryMapEntry memory_map[32];
u32 memory_map_entries = 0;
u32 total_usable_memory = 0;

// SimpleAllocator implementation
void SimpleAllocator::initialize(u32* start, u32 size) {
    memory_start = start;
    current_ptr = start;
    total_memory = size;
}

void* SimpleAllocator::allocate(u32 size) {
    size = (size + 3) & ~3;
    
    if ((u32)current_ptr + size > (u32)memory_start + total_memory) {
        return nullptr;
    }
    
    void* allocated = current_ptr;
    current_ptr = (u32*)((u32)current_ptr + size);
    return allocated;
}

u32 SimpleAllocator::get_used_memory() {
    return (u32)current_ptr - (u32)memory_start;
}

u32 SimpleAllocator::get_total_memory() {
    return total_memory;
}

void SimpleAllocator::reset() {
    current_ptr = memory_start;
}

// Memory detection functions
void detect_memory() {
    memory_map_entries = 0;
    total_usable_memory = 0;
    
    // Simulate BIOS memory detection (E820 style)
    // In real OS, this would use int 0x15, ax=0xE820
    
    // 0-640KB: Conventional memory (available)
    if (memory_map_entries < 32) {
        memory_map[memory_map_entries].base_addr = 0x00000000;
        memory_map[memory_map_entries].length = 0x0009F000; // 640KB - 4KB for BIOS
        memory_map[memory_map_entries].type = MEMORY_AVAILABLE;
        memory_map_entries++;
        total_usable_memory += 0x0009F000;
    }
    
    // 1MB-16MB: Extended memory (available for kernel)
    if (memory_map_entries < 32) {
        memory_map[memory_map_entries].base_addr = 0x00100000;
        memory_map[memory_map_entries].length = 0x00F00000; // 15MB
        memory_map[memory_map_entries].type = MEMORY_AVAILABLE;
        memory_map_entries++;
        total_usable_memory += 0x00F00000;
    }
    
    // 16MB-128MB: More extended memory
    if (memory_map_entries < 32) {
        memory_map[memory_map_entries].base_addr = 0x01000000;
        memory_map[memory_map_entries].length = 0x07000000; // 112MB
        memory_map[memory_map_entries].type = MEMORY_AVAILABLE;
        memory_map_entries++;
        total_usable_memory += 0x07000000;
    }
    
    // Add some reserved regions for realism
    if (memory_map_entries < 32) {
        memory_map[memory_map_entries].base_addr = 0x0009F000;
        memory_map[memory_map_entries].length = 0x00001000; // BIOS area
        memory_map[memory_map_entries].type = MEMORY_RESERVED;
        memory_map_entries++;
    }
    
    if (memory_map_entries < 32) {
        memory_map[memory_map_entries].base_addr = 0x000F0000;
        memory_map[memory_map_entries].length = 0x00010000; // System BIOS
        memory_map[memory_map_entries].type = MEMORY_RESERVED;
        memory_map_entries++;
    }
}

u32 get_total_usable_memory() {
    return total_usable_memory;
}

u32 get_total_memory_size() {
    return 0x400000; // 4MB for demo
}

void print_memory_map() {
    // This would be called from kernel to display memory map
    // Implementation depends on your kernel's output system
}

// Memory management functions
void initialize_memory() {
    // Detect available memory first
    detect_memory();
    
    // Use detected memory - start our allocator at 1MB with 3MB
    u32 memory_start_addr = 0x100000;
    u32 memory_size = 0x300000;
    
    g_allocator.initialize((u32*)memory_start_addr, memory_size);
}

void* kmalloc(u32 size) {
    return g_allocator.allocate(size);
}

void kfree(void* ptr) {
    // Simple bump allocator doesn't support free
    // Would be implemented in a more advanced allocator
}

// Utility functions
void itoa(char* buf, int value, int base) {
    static char digits[] = "0123456789ABCDEF";
    char* p = buf;
    int n = value;
    
    if (base == 10 && value < 0) {
        *p++ = '-';
        n = -value;
    }
    
    char* start = p;
    do {
        *p++ = digits[n % base];
        n /= base;
    } while (n > 0);
    *p-- = '\0';
    
    // Reverse the string
    while (start < p) {
        char temp = *start;
        *start = *p;
        *p = temp;
        start++;
        p--;
    }
}

int strlen(const char* str) {
    int len = 0;
    while (str[len] != 0) len++;
    return len;
}

// Memory analysis functions (for advanced use)
u32 find_largest_available_block() {
    u32 largest = 0;
    for (u32 i = 0; i < memory_map_entries; i++) {
        if (memory_map[i].type == MEMORY_AVAILABLE) {
            if (memory_map[i].length > largest) {
                largest = memory_map[i].length;
            }
        }
    }
    return largest;
}

u32 get_memory_map_entries() {
    return memory_map_entries;
}

const MemoryMapEntry* get_memory_map() {
    return memory_map;
}