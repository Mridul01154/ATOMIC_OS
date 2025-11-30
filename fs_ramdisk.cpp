#include "fs_ramdisk.h"
#include "memory.h"

// Global RAM disk instance
RAMDiskFS g_ramdisk;

// Simple string copy
static void copy_str(char* dst, const char* src) {
    while (*src) {
        *dst++ = *src++;
    }
    *dst = 0;
}

// Simple string compare
static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// RAMDiskFS Implementation
bool RAMDiskFS::initialize(u32 memory_address, u32 size) {
    disk_memory = (u8*)memory_address;
    total_size = size;
    
    // Calculate layout
    u32 superblock_size = sizeof(RAMDiskSuperblock);
    u32 fat_size = (size / RAMDISK_BLOCK_SIZE); // 1 byte per block
    u32 file_table_size = RAMDISK_MAX_FILES * sizeof(RAMDiskFileEntry);
    
    // Set pointers
    superblock = (RAMDiskSuperblock*)disk_memory;
    fat = disk_memory + superblock_size;
    file_table = (RAMDiskFileEntry*)(fat + fat_size);
    data_blocks = (u8*)(file_table + file_table_size);
    
    // Format the disk
    return format();
}

bool RAMDiskFS::format() {
    // Initialize superblock
    copy_str(superblock->magic, RAMDISK_MAGIC);
    superblock->version = RAMDISK_VERSION;
    superblock->block_size = RAMDISK_BLOCK_SIZE;
    
    // Calculate blocks
    u32 total_blocks = (total_size - (u32)(data_blocks - disk_memory)) / RAMDISK_BLOCK_SIZE;
    superblock->total_blocks = total_blocks;
    superblock->free_blocks = total_blocks;
    superblock->file_count = 0;
    superblock->data_blocks = total_blocks;
    
    // Clear FAT (0 = free block)
    for (u32 i = 0; i < total_blocks; i++) {
        fat[i] = 0;
    }
    
    // Clear file table
    for (u32 i = 0; i < RAMDISK_MAX_FILES; i++) {
        file_table[i].filename[0] = 0;
        file_table[i].start_block = 0;
        file_table[i].size = 0;
        file_table[i].timestamp = 0;
        file_table[i].type = 0;
    }
    
    return true;
}

u32 RAMDiskFS::find_free_block() {
    for (u32 i = 0; i < superblock->total_blocks; i++) {
        if (fat[i] == 0) {
            return i;
        }
    }
    return (u32)-1;
}

u32 RAMDiskFS::calculate_blocks_needed(u32 file_size) {
    return (file_size + RAMDISK_BLOCK_SIZE - 1) / RAMDISK_BLOCK_SIZE;
}

RAMDiskFileEntry* RAMDiskFS::find_file_entry(const char* filename) {
    for (u32 i = 0; i < RAMDISK_MAX_FILES; i++) {
        if (file_table[i].filename[0] != 0 && 
            strcmp(file_table[i].filename, filename) == 0) {
            return &file_table[i];
        }
    }
    return nullptr;
}

RAMDiskFileEntry* RAMDiskFS::find_free_file_entry() {
    for (u32 i = 0; i < RAMDISK_MAX_FILES; i++) {
        if (file_table[i].filename[0] == 0) {
            return &file_table[i];
        }
    }
    return nullptr;
}

bool RAMDiskFS::create_file(const char* filename, const u8* data, u32 size) {
    if (!filename || !data || size == 0) {
        return false;
    }
    
    // Delete existing file if it exists
    RAMDiskFileEntry* existing_entry = find_file_entry(filename);
    if (existing_entry != nullptr) {
        delete_file(filename);
    }
    
    // Find free file entry
    RAMDiskFileEntry* entry = find_free_file_entry();
    if (!entry) {
        return false;
    }
    
    // Calculate blocks needed
    u32 blocks_needed = calculate_blocks_needed(size);
    if (blocks_needed > superblock->free_blocks) {
        return false;
    }
    
    // Find any free blocks (not necessarily contiguous)
    u32 blocks_found = 0;
    u32 allocated_blocks[blocks_needed];
    
    for (u32 i = 0; i < superblock->total_blocks && blocks_found < blocks_needed; i++) {
        if (fat[i] == 0) {
            allocated_blocks[blocks_found] = i;
            blocks_found++;
        }
    }
    
    if (blocks_found < blocks_needed) {
        return false;
    }
    
    // Allocate blocks
    for (u32 i = 0; i < blocks_needed; i++) {
        fat[allocated_blocks[i]] = 1;
    }
    
    // Fill file entry
    copy_str(entry->filename, filename);
    entry->start_block = allocated_blocks[0];
    entry->size = size;
    entry->timestamp = 0;
    entry->type = 0;
    
    // Copy data to blocks (handle non-contiguous blocks)
    u32 bytes_copied = 0;
    for (u32 block_idx = 0; block_idx < blocks_needed && bytes_copied < size; block_idx++) {
        u8* dest = data_blocks + (allocated_blocks[block_idx] * RAMDISK_BLOCK_SIZE);
        u32 bytes_in_this_block = (size - bytes_copied) < RAMDISK_BLOCK_SIZE ? 
                                 (size - bytes_copied) : RAMDISK_BLOCK_SIZE;
        
        for (u32 i = 0; i < bytes_in_this_block; i++) {
            dest[i] = data[bytes_copied + i];
        }
        bytes_copied += bytes_in_this_block;
    }
    
    // Update superblock
    superblock->file_count++;
    superblock->free_blocks -= blocks_needed;
    
    return true;
}

bool RAMDiskFS::read_file(const char* filename, u8* buffer, u32 buffer_size) {
    if (!filename || !buffer) return false;
    
    RAMDiskFileEntry* entry = find_file_entry(filename);
    if (!entry) return false;
    
    if (buffer_size < entry->size) return false;
    
    // Simple approach: assume blocks are contiguous for reading
    u8* src = data_blocks + (entry->start_block * RAMDISK_BLOCK_SIZE);
    
    for (u32 i = 0; i < entry->size; i++) {
        buffer[i] = src[i];
    }
    
    return true;
}

bool RAMDiskFS::delete_file(const char* filename) {
    RAMDiskFileEntry* entry = find_file_entry(filename);
    if (!entry) return false;
    
    // Free blocks in FAT
    u32 blocks_needed = calculate_blocks_needed(entry->size);
    for (u32 i = 0; i < blocks_needed; i++) {
        fat[entry->start_block + i] = 0;
    }
    
    // Clear file entry
    entry->filename[0] = 0;
    entry->start_block = 0;
    entry->size = 0;
    entry->timestamp = 0;
    entry->type = 0;
    
    // Update superblock
    superblock->file_count--;
    superblock->free_blocks += blocks_needed;
    
    return true;
}

bool RAMDiskFS::file_exists(const char* filename) {
    return find_file_entry(filename) != nullptr;
}
// Add to fs_ramdisk.cpp
bool fs_file_exists(const char* filename) {
    return g_ramdisk.file_exists(filename);
}
void RAMDiskFS::list_files() {
    // Simple file listing - will be enhanced later
    for (u32 i = 0; i < RAMDISK_MAX_FILES; i++) {
        if (file_table[i].filename[0] != 0) {
            // In real implementation, this would print to screen
            // For now, it's a placeholder
        }
    }
}

u32 RAMDiskFS::get_file_count() {
    return superblock->file_count;
}

u32 RAMDiskFS::get_free_space() {
    return superblock->free_blocks * RAMDISK_BLOCK_SIZE;
}

u32 RAMDiskFS::get_total_space() {
    return superblock->total_blocks * RAMDISK_BLOCK_SIZE;
}

bool RAMDiskFS::is_initialized() {
    return disk_memory != nullptr;
}

void RAMDiskFS::get_file_info(const char* filename, u32* size, u32* timestamp) {
    RAMDiskFileEntry* entry = find_file_entry(filename);
    if (entry) {
        if (size) *size = entry->size;
        if (timestamp) *timestamp = entry->timestamp;
    }
}

// Debug function to check RAM disk status
void RAMDiskFS::debug_status() {
    // This would display debug info on screen
    // Implementation depends on your kernel's output system
}

// Public interface functions
void fs_initialize() {
    // Reserve 1MB for RAM disk and initialize it
    u32 ramdisk_addr = 0x200000; // 2MB mark - after kernel
    g_ramdisk.initialize(ramdisk_addr, RAMDISK_DEFAULT_SIZE);
}

bool fs_create_file(const char* filename, const u8* data, u32 size) {
    return g_ramdisk.create_file(filename, data, size);
}

bool fs_read_file(const char* filename, u8* buffer, u32 buffer_size) {
    return g_ramdisk.read_file(filename, buffer, buffer_size);
}

bool fs_delete_file(const char* filename) {
    return g_ramdisk.delete_file(filename);
}

void fs_list_files() {
    g_ramdisk.list_files();
}

u32 fs_get_free_space() {
    return g_ramdisk.get_free_space();
}



void fs_debug_status() {
    g_ramdisk.debug_status();
}

// File listing with actual display - FIXED VERSION
int RAMDiskFS::get_file_list(RAMDiskFileEntry* list, int max_entries) {
    int count = 0;
    for (u32 i = 0; i < RAMDISK_MAX_FILES && count < max_entries; i++) {
        if (file_table[i].filename[0] != 0) {
            copy_str(list[count].filename, file_table[i].filename);
            list[count].start_block = file_table[i].start_block;
            list[count].size = file_table[i].size;
            list[count].timestamp = file_table[i].timestamp;
            list[count].type = file_table[i].type;
            count++;
        }
    }
    return count;
}

int fs_get_file_list(RAMDiskFileEntry* list, int max_entries) {
    return g_ramdisk.get_file_list(list, max_entries);
}
