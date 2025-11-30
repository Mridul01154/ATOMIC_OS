#ifndef FS_RAMDISK_H
#define FS_RAMDISK_H

#include "memory.h"

// RAM Disk Constants
#define RAMDISK_MAGIC "ATOMICFS"
#define RAMDISK_VERSION 1
#define RAMDISK_DEFAULT_SIZE (1024 * 1024)  // 1MB
#define RAMDISK_BLOCK_SIZE 1024             // 1KB blocks
#define RAMDISK_MAX_FILES 64
#define RAMDISK_FILENAME_LEN 32

// RAM Disk Structures
struct RAMDiskSuperblock {
    char magic[8];
    u32 version;
    u32 total_blocks;
    u32 free_blocks;  
    u32 file_count;
    u32 block_size;
    u32 fat_blocks;
    u32 file_table_blocks;
    u32 data_blocks;
    u8 reserved[476];
};

struct RAMDiskFileEntry {
    char filename[RAMDISK_FILENAME_LEN];
    u32 start_block;
    u32 size;
    u32 timestamp;
    u8 type;
    u8 reserved[15];
};

class RAMDiskFS {
private:
    u8* disk_memory;
    u32 total_size;
    RAMDiskSuperblock* superblock;
    u8* fat;
    RAMDiskFileEntry* file_table;
    u8* data_blocks;
    
    // Helper methods
    u32 find_free_block();
    u32 calculate_blocks_needed(u32 file_size);
    RAMDiskFileEntry* find_file_entry(const char* filename);
    RAMDiskFileEntry* find_free_file_entry();

public:
    // Core operations
    bool initialize(u32 memory_address, u32 size = RAMDISK_DEFAULT_SIZE);
    bool format();
    
    // File operations
    bool create_file(const char* filename, const u8* data, u32 size);
    bool read_file(const char* filename, u8* buffer, u32 buffer_size);
    bool delete_file(const char* filename);
    bool file_exists(const char* filename);  // <-- ADD THIS LINE
    
    // Directory operations  
    void list_files();
    u32 get_file_count();
    u32 get_free_space();
    u32 get_total_space();
    int get_file_list(RAMDiskFileEntry* list, int max_entries);  // <-- MOVE THIS HERE
    
    // Utility
    bool is_initialized();
    void get_file_info(const char* filename, u32* size, u32* timestamp);
    void debug_status();
};

extern RAMDiskFS g_ramdisk;

// Public interface functions
void fs_initialize();
bool fs_create_file(const char* filename, const u8* data, u32 size);
bool fs_read_file(const char* filename, u8* buffer, u32 buffer_size);
bool fs_delete_file(const char* filename);
bool fs_file_exists(const char* filename);  // <-- ADD THIS LINE
void fs_list_files();
u32 fs_get_free_space();
void fs_debug_status();
int fs_get_file_list(RAMDiskFileEntry* list, int max_entries);

#endif