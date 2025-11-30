#ifndef MEMORY_H
#define MEMORY_H
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef unsigned long long u64;

// Memory map entry structure
struct MemoryMapEntry {
    u64 base_addr;
    u64 length;
    u32 type;
    u32 extended_attributes;
};

// Memory types
#define MEMORY_AVAILABLE 1
#define MEMORY_RESERVED 2
#define MEMORY_ACPI_RECLAIM 3
#define MEMORY_ACPI_NVS 4
#define MEMORY_BAD 5

// Basic allocator class
class SimpleAllocator {
private:
    u32* memory_start;
    u32* current_ptr;
    u32 total_memory;
    
public:
    void initialize(u32* start, u32 size);
    void* allocate(u32 size);
    u32 get_used_memory();
    u32 get_total_memory();
    void reset();
};

// Paging structures
struct PageDirectoryEntry {
    u32 present : 1;
    u32 read_write : 1;
    u32 user_supervisor : 1;
    u32 write_through : 1;
    u32 cache_disable : 1;
    u32 accessed : 1;
    u32 reserved : 1;
    u32 page_size : 1;
    u32 global : 1;
    u32 available : 3;
    u32 page_table_base : 20;
};

struct PageTableEntry {
    u32 present : 1;
    u32 read_write : 1;
    u32 user_supervisor : 1;
    u32 write_through : 1;
    u32 cache_disable : 1;
    u32 accessed : 1;
    u32 dirty : 1;
    u32 reserved : 1;
    u32 global : 1;
    u32 available : 3;
    u32 page_base : 20;
};

// Global allocator instance
extern SimpleAllocator g_allocator;

// Memory detection
extern MemoryMapEntry memory_map[32];
extern u32 memory_map_entries;
extern u32 total_usable_memory;

// Function declarations
void initialize_memory();
void fs_initialize();
void detect_memory();
u32 get_total_usable_memory();
u32 get_total_memory_size();
void print_memory_map();
void* kmalloc(u32 size);
void kfree(void* ptr);
void itoa(char* buf, int value, int base);
int strlen(const char* str);

// Advanced memory functions
u32 find_largest_available_block();
u32 get_memory_map_entries();
const MemoryMapEntry* get_memory_map();

#endif