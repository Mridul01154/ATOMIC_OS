// atomic_kernel.cpp
// Single-file improved "ATOMIC OS" kernel UI - 32-bit freestanding
extern "C" void main();
#include "memory.h"
#include "fs_ramdisk.h"

// VGA constants
static const int WIDTH = 80;
static const int HEIGHT = 25;
typedef unsigned short u16;
static volatile u16* const VGA = (volatile u16*)0xB8000;

// I/O ports
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

// Basic types
typedef unsigned char u8;
typedef unsigned short u16t;
typedef unsigned int u32;

// --- Low-level I/O helpers (fixed inline asm ordering and constraints) ---
static inline void outb(u16t port, u8 value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline u8 inb(u16t port) {
    u8 ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Make a VGA cell
static inline u16 vga_entry(char c, unsigned char attr) {
    return (u16)c | ((u16)attr << 8);
}

// Write one character safely
static inline void putc_xy(int x, int y, char c, unsigned char attr) {
    if (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return;
    VGA[y * WIDTH + x] = vga_entry(c, attr);
}

// Clear full screen
void clear_screen(unsigned char attr) {
    u16 cell = vga_entry(' ', attr);
    for (int i = 0; i < WIDTH * HEIGHT; i++) VGA[i] = cell;
}

// Print string at (x,y)
void print_string(const char* s, int x, int y, unsigned char attr = 0x0F) {
    for (int i = 0; s[i] != 0 && x + i < WIDTH; i++) putc_xy(x + i, y, s[i], attr);
}

// Draw centered text
void print_centered(const char* s, int y, unsigned char attr = 0x0F) {
    int len = 0;
    while (s[len] != 0) len++;
    int x = (WIDTH - len) / 2;
    print_string(s, x, y, attr);
}

// Safe strcmp (same as before)
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// --- CMOS / RTC helpers (fixed) ---
u8 read_cmos(u8 reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

int is_updating_rtc() {
    outb(CMOS_ADDRESS, 0x0A);
    return inb(CMOS_DATA) & 0x80;
}

u8 bcd_to_bin(u8 bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

struct Time {
    u8 second;
    u8 minute;
    u8 hour;
    u8 day;
    u8 month;
    u8 year; // two-digit year (00..99)
};

// Stable RTC read: read twice and compare (common OSDev pattern)
Time read_rtc_time() {
    Time t1, t2;
    u8 reg_b;
    do {
        // wait while update in progress
        while (is_updating_rtc()) {}

        t1.second = read_cmos(0x00);
        t1.minute = read_cmos(0x02);
        t1.hour   = read_cmos(0x04);
        t1.day    = read_cmos(0x07);
        t1.month  = read_cmos(0x08);
        t1.year   = read_cmos(0x09);

        // read twice
        while (is_updating_rtc()) {}

        t2.second = read_cmos(0x00);
        t2.minute = read_cmos(0x02);
        t2.hour   = read_cmos(0x04);
        t2.day    = read_cmos(0x07);
        t2.month  = read_cmos(0x08);
        t2.year   = read_cmos(0x09);
    } while (t1.second != t2.second || t1.minute != t2.minute || t1.hour != t2.hour ||
             t1.day != t2.day || t1.month != t2.month || t1.year != t2.year);

    // Now get register B once for format info
    outb(CMOS_ADDRESS, 0x0B);
    reg_b = inb(CMOS_DATA);

    Time result = t1;
    // Convert from BCD if necessary
    if (!(reg_b & 0x04)) {
        result.second = bcd_to_bin(result.second);
        result.minute = bcd_to_bin(result.minute);
        result.hour   = bcd_to_bin(result.hour & 0x7F); // clear possible 12-hour bit for conversion
        result.day    = bcd_to_bin(result.day);
        result.month  = bcd_to_bin(result.month);
        result.year   = bcd_to_bin(result.year);
    } else {
        // If reg_b & 0x04, values are already binary - ensure hour cleared of 12h bit before checking
        result.hour = result.hour & 0x7F;
    }

    // Convert 12-hour to 24-hour format if necessary (bit 1 of reg_b == 0 => 12-hour mode)
    if (!(reg_b & 0x02) && (t1.hour & 0x80)) {
        // hour's PM bit was set in t1; convert using t1 because it contained the 0x80 marker
        u8 hour12 = t1.hour & 0x7F;
        // if BCD, hour12 was converted earlier; we used t1.hour to check 0x80 but used result.hour for numeric
        result.hour = (hour12 % 12) + 12;
        if (result.hour >= 24) result.hour -= 24;
    }

    return result;
}

// formatting helpers
static inline void two_digits(char* dst, int v) {
    dst[0] = '0' + (v / 10) % 10;
    dst[1] = '0' + (v % 10);
}

// Format time as string "HH:MM:SS"
void format_time(char* buffer, const Time& time) {
    two_digits(&buffer[0], time.hour);
    buffer[2] = ':';
    two_digits(&buffer[3], time.minute);
    buffer[5] = ':';
    two_digits(&buffer[6], time.second);
    buffer[8] = 0;
}

// Format date as string "DD/MM/20YY"
void format_date(char* buffer, const Time& time) {
    two_digits(&buffer[0], time.day);
    buffer[2] = '/';
    two_digits(&buffer[3], time.month);
    buffer[5] = '/';
    buffer[6] = '2';
    buffer[7] = '0';
    two_digits(&buffer[8], time.year);
    buffer[10] = 0;
}

// Draw glowing horizontal line
void draw_glow_line(int y, unsigned char attr) {
    for (int x = 0; x < WIDTH; x++) putc_xy(x, y, 0xDB, attr);
}

void draw_atomic_border() {
    int w = 64;
    int h = 7;

    // Top border
    putc_xy(15, 1, '+', 0x1F);
    for (int x = 16; x < w - 1; x++)
        putc_xy(x, 1, '-', 0x1F);   // use '_' if you want
    putc_xy(w - 1, 1, '+', 0x1F);

    // Side borders
    for (int y = 2; y < h ; y++) {
        putc_xy(15, y, '|', 0x1F);
        putc_xy(w - 1, y, '|', 0x1F);
    }

    // Bottom border
    putc_xy(15, h  , '+', 0x1F);
    for (int x = 16; x < w - 1; x++)
        putc_xy(x, h , '-', 0x1F);
    putc_xy(w - 1, h , '+', 0x1F);
}


// Draw static interface skeleton (draw once)
void draw_static_interface() {
    // Background subtle grid but tiled with the word "ATOMIC" as a pattern
    const char *tile = "ATOMIC ";
    int tile_len = 7; // length of tile string above

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            // base background (dark teal-ish)
            unsigned char bg = 0x10;

            // slight checker to add depth
            if ((x + y) % 6 == 0) bg = 0x11;

            // draw the tile-letter in a faint color occasionally to form the repeating word pattern
            char ch = tile[(x + y) % tile_len];
            if ((x % 8) == 0 && (y % 3) == 0) {
                // faint character as background motif
                putc_xy(x, y, ch, 0x12); // dark green on dark background (subtle)
            } else {
                putc_xy(x, y, ' ', bg);
            }
        }
    }

    // Bold centered ASCII wordmark for "ATOMIC OS"
    const char* banner[] = {
        "   ___   _____  _____  __  __  _____  _____  ",
        "  / _ \\ |_   _||  _  ||  \\/  ||_   _|| ____| ",
        " | |_| |  | |  | | | || |\\/| |  | |  | |     ",
        " | | | |  | |  | |_| || |  | | _| |_ | |___  ",
        " |_| |_|  |_|  |_____||_|  |_||_____||_____| ",
        "                                              "
    };
    int banner_h = sizeof(banner) / sizeof(banner[0]);
    int banner_y = 2; // start a little lower so header glow remains
    for (int i = 0; i < banner_h; ++i) {
        print_centered(banner[i], banner_y + i, 0x3E); // cyan-on-blueish glow
    }

    // === FLOATING PLATFORM (improved) ===
    int platform_y = banner_y + banner_h + 0; // position platform under banner
    
    draw_atomic_border();
    
    // === LARGE WORDMARK (centered, stylized) ===
    // A compact, readable "ATOMIC OS" wordmark appearing above the platform
    print_centered("   A T O M I C   O S   ", platform_y , 0x1E);
    print_centered("   SYSTEM READY        ", platform_y + 7, 0x1F);

    // === HOLOGRAPHIC INFO DISPLAY BOX (refined) ===
    int box_w = 52;
    int box_x = (WIDTH - box_w) / 2;
    int box_y = platform_y ;
    int box_h = 6;



    // Holographic interior: a subtle gradient using two attributes
    for (int y = box_y + 1; y < box_y + box_h - 1; ++y) {
        unsigned char attr = (y % 2 == 0) ? 0x1E : 0x1F; // alternate to simulate depth
        for (int x = box_x + 1; x < box_x + box_w - 1; ++x) {
            putc_xy(x, y, ' ', attr);
        }
    }

    // Static labels inside box (left-aligned)
    print_string("  CPU:    32-bit x86", box_x + 2, box_y + 2, 0x1E);
    print_string("  Kernel: v0.1.0     ", box_x + 2, box_y + 3, 0x1E);
    print_string("  Status: OPERATIONAL", box_x + 2, box_y + 4, 0x1E);

    // Reserve a right-side small 'hologram' panel inside the box for icons/indicators
    int panel_x = box_x + box_w - 12;
    print_string("[IO] OK", panel_x, box_y + 2, 0x1E);
    print_string("[NET] --", panel_x, box_y + 3, 0x1E);

    // === TERMINAL AREA & INPUT FIELD (improved) ===
    print_centered("TERMINAL READY - TYPE COMMANDS BELOW", box_y + box_h + 2, 0x17);
    print_centered("TYPE 'help' FOR FURTHER INFO", box_y + box_h + 3, 0x17);
}


/// Update the header time and holographic time (separate function, avoids full redraw)
void update_time_display() {
    Time now = read_rtc_time();
    char time_str[9], date_str[11];
    format_time(time_str, now);
    format_date(date_str, now);

    // Right side of header
    // Clear area first (simple background)
    for (int x = 55; x < 79; x++) putc_xy(x, 0, ' ', 0x30);
    print_string(date_str, 56, 0, 0x3F);
    print_string(time_str, 70, 0, 0x3F);


    
}

// --- Keyboard reading and mapping (safer) ---
unsigned char read_scan_code() {
    unsigned char status;
    do {
        status = inb(KEYBOARD_STATUS_PORT);
    } while (!(status & 1)); // wait until output buffer full
    return inb(KEYBOARD_DATA_PORT);
}

// Simple set 1 scancode -> ASCII map (index by scancode, 0..127)
char scan_code_to_ascii(unsigned char scan_code) {
    if (scan_code & 0x80) return 0; // key release
    
    static bool shift_pressed = false;
    static bool caps_lock = false;
    
    
    // Handle modifier keys
    if (scan_code == 0x2A || scan_code == 0x36) {
        shift_pressed = true;
        return 0;
    }
    if (scan_code == 0xAA || scan_code == 0xB6) {
        shift_pressed = false;
        return 0;
    }
    if (scan_code == 0x3A) {
        caps_lock = !caps_lock;
        return 0;
    }
    
    // Determine if we should use uppercase
    bool uppercase = shift_pressed ^ caps_lock; // XOR: if either is true but not both
    
    // Keyboard map with shift support
    static const char keyboard_map[128] = {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=',0,0,
        'q','w','e','r','t','y','u','i','o','p','[',']', '\n',0,'a','s',
        'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
        'b','n','m',',','.','/',0,'*',0,' ',
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,  // 58-67
        0,0,0,0,0,0,0,0,0,0  
    };
    
    static const char keyboard_map_shift[128] = {
        0, 27, '!','@','#','$','%','^','&','*','(',')','_','+',0,0,
        'Q','W','E','R','T','Y','U','I','O','P','{','}', '\n',0,'A','S',
        'D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V',
        'B','N','M','<','>','?',0,'*',0,' ',
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,  // 58-67
        0,0,0,0,0,0,0,0,0,0  
    };
    
    if (scan_code < sizeof(keyboard_map)) {
        if (uppercase && scan_code < sizeof(keyboard_map_shift)) {
            return keyboard_map_shift[scan_code];
        } else {
            return keyboard_map[scan_code];
        }
    }
    return 0;
}

// Small safe helpers
void copy_str(char* dst, const char* src) {
    while (*src) { *dst++ = *src++; }
    *dst = 0;
}

// Word wrap function - breaks long text into multiple lines
void show_output_wrapped(const char* text, unsigned char attr = 0x17) {
    // Clear area first
    for (int y = 19; y <= 21; y++) {
        for (int x = 16; x < 64; x++) {
            putc_xy(x, y, ' ', 0x10);
        }
    }
    
    int max_length = 48;  // Maximum characters per line (64-16=48 width)
    int current_line = 0;
    int start_pos = 0;
    int text_len = strlen(text);
    
    while (start_pos < text_len && current_line < 3) {  // Max 3 lines
        // Find where to break the line
        int end_pos = start_pos + max_length;
        if (end_pos > text_len) {
            end_pos = text_len;
        } else {
            // Try to break at a space if possible
            for (int i = end_pos; i > start_pos; i--) {
                if (text[i] == ' ' || text[i] == ',') {
                    end_pos = i;
                    break;
                }
            }
        }
        
        // Extract and display this line
        char line[50];
        int line_len = end_pos - start_pos;
        for (int i = 0; i < line_len; i++) {
            line[i] = text[start_pos + i];
        }
        line[line_len] = 0;
        
        print_centered(line, 20 + current_line, attr);
        
        start_pos = end_pos;
        // Skip spaces at the beginning of next line
        while (start_pos < text_len && text[start_pos] == ' ') {
            start_pos++;
        }
        current_line++;
    }
}

// --- Enhanced Text Editor with Memory Save/Load ---
class TextEditor {
private:
    char buffer[20000];
    int cursor_pos;
    bool ctrl_pressed;
     char current_filename[50];

     void show_file_browser(bool for_saving = false) {
        clear_screen(0x10);
        
        if (for_saving) {
            print_centered(" SAVE AS - ENTER FILENAME ", 0, 0x1F);
        } else {
            print_centered(" LOAD FILE - SELECT FILE ", 0, 0x1F);
        }
        
        print_centered("UP/DOWN: NAVIGATE | ENTER: SELECT | ESC: CANCEL", 1, 0x17);
        
        // List files
        RAMDiskFileEntry files[16];
        int file_count = fs_get_file_list(files, 16);
        
        if (file_count == 0) {
            print_centered("No files found", 5, 0x47);
        } else {
            for (int i = 0; i < file_count && i < 15; i++) {
                char file_line[60];
                copy_str(file_line, "  ");
                copy_str(file_line + 2, files[i].filename);
                copy_str(file_line + 2 + strlen(files[i].filename), " (");
                char size_str[10];
                itoa(size_str, files[i].size, 10);
                copy_str(file_line + 2 + strlen(files[i].filename) + 2, size_str);
                copy_str(file_line + 2 + strlen(files[i].filename) + 2 + strlen(size_str), " bytes)");
                
                print_string(file_line, 10, 3 + i, 0x17);
            }
        }
        
        // If saving, show filename input
        if (for_saving) {
            print_centered("Or enter new filename:", 18, 0x1E);
            print_string("Filename: > ", 20, 20, 0x1F);
            putc_xy(32, 20, '_', 0x4F); // Cursor
        }
        
        // Wait for user input
        if (for_saving) {
            get_filename_input();
        } else {
            select_file_from_list(file_count);
        }
    }
    
    void get_filename_input() {
        char new_filename[50] = {0};
        int filename_pos = 0;
        
        while (true) {
            unsigned char scan_code = read_scan_code();
            
            if (scan_code == 0x01) { // ESC
                break;
            } else if (scan_code == 0x1C) { // ENTER
                if (filename_pos > 0) {
                    save_file_as(new_filename);
                }
                break;
            } else if (scan_code == 0x0E) { // BACKSPACE
                if (filename_pos > 0) {
                    filename_pos--;
                    new_filename[filename_pos] = 0;
                }
            } else {
                char ascii = scan_code_to_ascii(scan_code);
                if (ascii != 0 && filename_pos < 49) {
                    new_filename[filename_pos++] = ascii;
                    new_filename[filename_pos] = 0;
                }
            }
            
            // Update display
            for (int x = 32; x < 70; x++) {
                putc_xy(x, 20, ' ', 0x1F);
            }
            print_string(new_filename, 32, 20, 0x1F);
            putc_xy(32 + filename_pos, 20, '_', 0x4F);
        }
    }
    
    void select_file_from_list(int file_count) {
        if (file_count == 0) {
            // Wait for any key
            read_scan_code();
            return;
        }
        
        int selected = 0;
        
        while (true) {
            // Highlight selected file
            for (int i = 0; i < file_count && i < 15; i++) {
                unsigned char attr = (i == selected) ? 0x4F : 0x17;
                
                RAMDiskFileEntry files[16];
                fs_get_file_list(files, 16);
                
                char file_line[60];
                copy_str(file_line, "  ");
                copy_str(file_line + 2, files[i].filename);
                
                // Clear line and print with highlight
                for (int x = 10; x < 70; x++) {
                    putc_xy(x, 3 + i, ' ', attr);
                }
                print_string(file_line, 10, 3 + i, attr);
            }
            
            unsigned char scan_code = read_scan_code();
            
            if (scan_code == 0x01) { // ESC
                break;
            } else if (scan_code == 0x1C) { // ENTER
                RAMDiskFileEntry files[16];
                fs_get_file_list(files, 16);
                load_file(files[selected].filename);
                break;
            } else if (scan_code == 0x48) { // UP
                if (selected > 0) selected--;
            } else if (scan_code == 0x50) { // DOWN
                if (selected < file_count - 1) selected++;
            }
        }
    }

public:
    TextEditor() : cursor_pos(0), ctrl_pressed(false) {
        // Initialize buffer
        for (int i = 0; i < 20000; i++) buffer[i] = 0;
        current_filename[0] = 0;
    }

    // Add these methods to TextEditor class:
   void load_file(const char* filename) {
        u8 file_buffer[19000];
        if (fs_read_file(filename, file_buffer, sizeof(file_buffer))) {
            // Clear current buffer
            for (int i = 0; i < 20000; i++) buffer[i] = 0;
            
            // Copy file content to editor buffer
            int i = 0;
            while (file_buffer[i] != 0 && i < 19999) {
                buffer[i] = file_buffer[i];
                i++;
            }
            buffer[i] = 0;
            
            cursor_pos = 0;
            copy_str(current_filename, filename);
            show_message("File loaded", 0x1E);
        } else {
            show_message("Load failed", 0x47);
        }
        refresh_display();
    }

    void save_file() {
        if (current_filename[0] == 0) {
            show_message("No filename - use F2 to save as", 0x47);
            return;
        }
        
        if (fs_create_file(current_filename, (const u8*)buffer, strlen(buffer))) {
            show_message("File saved", 0x1E);
        } else {
            show_message("Save failed", 0x47);
        }
    }

    void save_file_as(const char* filename) {
        copy_str(current_filename, filename);
        save_file();
    }

    void draw_editor() {
        clear_screen(0x10);

        // Simple editor header
        print_centered(" ATOMIC TEXT EDITOR - TYPE TO EDIT ", 0, 0x1F);
        print_centered("ESC: EXIT | F1: CLEAR | F2: SAVE AS | F3: SAVE | F4: LOAD | Arrow keys: NAVIGATE", 1, 0x17);

        refresh_display();
    }

    void refresh_display() {
        // Clear content area
        for (int y = 3; y < HEIGHT - 2; y++) {
            for (int x = 2; x < WIDTH - 2; x++) {
                putc_xy(x, y, ' ', 0x10);
            }
        }
        
        // Display text content
        int x = 2, y = 3;
        for (int i = 0; buffer[i] != 0 && y < HEIGHT - 2; i++) {
            if (buffer[i] == '\n') { 
                x = 2; 
                y++; 
            } else { 
                putc_xy(x, y, buffer[i], 0x1F); 
                x++; 
                if (x >= WIDTH - 2) { 
                    x = 2; 
                    y++; 
                } 
            }
        }
        
        // Display cursor
        int cur_x = 2, cur_y = 3;
        int line_start = 0;
        for (int i = 0; i < cursor_pos && buffer[i] != 0; i++) {
            if (buffer[i] == '\n') { 
                cur_x = 2; 
                cur_y++; 
                line_start = i + 1;
            } else { 
                cur_x++; 
                if (cur_x >= WIDTH - 2) { 
                    cur_x = 2; 
                    cur_y++; 
                    line_start = i + 1;
                } 
            }
        }
        putc_xy(cur_x, cur_y, '_', 0x4F);
        
    }

    void handle_input(unsigned char scan_code) {
        // Handle modifier keys first
        if (scan_code == 0x1D) {  // Ctrl press
            ctrl_pressed = true;
            return;
        } else if (scan_code == 0x9D) {  // Ctrl release
            ctrl_pressed = false;
            return;
        }
        
        char ascii = scan_code_to_ascii(scan_code);
        
        // Special key handling
        if (scan_code == 0x01) return;  // ESC (handled in run loop)
        
        // Clear screen (F1)
        else if (scan_code == 0x3B) {  // F1 - Clear screen
            for (int i = 0; i < 20000; i++) buffer[i] = 0;
            cursor_pos = 0;
            show_message("SCREEN CLEARED", 0x1E);
            return;
        }
        else if (ctrl_pressed && scan_code == 0x1F) {   
            for (int i = 0; i < 20000; i++) buffer[i] = 0;
            cursor_pos = 0;
            show_message("SCREEN CLEARED", 0x1E);
            return;
        }
        // Arrow keys - Extended scan codes
        else if (scan_code == 0x48) {  // Up arrow
            move_cursor_up();
            return;
        }
        else if (scan_code == 0x50) {  // Down arrow
            move_cursor_down();
            return;
        }
        else if (scan_code == 0x4B) {  // Left arrow
            if (cursor_pos > 0) {
                cursor_pos--;
            }
        }
        else if (scan_code == 0x4D) {  // Right arrow
            if (cursor_pos < 19999 && buffer[cursor_pos] != 0) {
                cursor_pos++;
            }
        }

        else if (scan_code == 0x3C) { // F2 - Save As
            show_file_browser(true); // true = for saving
            draw_editor(); // Redraw editor after file operation
            return;
        } else if (scan_code == 0x3D) { // F3 - Save
            save_file();
            return;
        } else if (scan_code == 0x3E) { // F4 - Load
            show_file_browser(false); // false = for loading
            draw_editor(); // Redraw editor after file operation
            return;
        } else if (scan_code == 0x3F) { // F5 - New
            // Clear buffer and reset filename
            for (int i = 0; i < 20000; i++) buffer[i] = 0;
            current_filename[0] = 0;
            cursor_pos = 0;
            show_message("New file created", 0x1E);
            refresh_display();
            return;
        }
        
        // Backspace
        else if (scan_code == 0x0E) {   
            if (cursor_pos > 0) {
                // Shift all characters left
                for (int i = cursor_pos - 1; i < 19999; i++) {
                    buffer[i] = buffer[i + 1];
                }
                cursor_pos--;
                buffer[1999] = 0; // Ensure null terminator
            }
        } 
        
        // Enter
        else if (scan_code == 0x1C) { 
            if (cursor_pos < 19999) {
                // Shift all characters right to make space for newline
                for (int i = 19998; i > cursor_pos; i--) {
                    buffer[i] = buffer[i - 1];
                }
                buffer[cursor_pos++] = '\n';
                buffer[19999] = 0; // Ensure null terminator
            }
        } 
        
        // Text input
        else if (ascii != 0 && cursor_pos < 19999) {
            // Shift all characters right to make space for new character
            for (int i = 19998; i > cursor_pos; i--) {
                buffer[i] = buffer[i - 1];
            }
            buffer[cursor_pos++] = ascii;
            buffer[19999] = 0; // Ensure null terminator
        }
        
        refresh_display();
    }

    void move_cursor_up() {
        if (cursor_pos == 0) return;
        
        // Find current line start
        int current_line_start = find_line_start(cursor_pos);
        if (current_line_start == 0) return; // Already at first line
        
        // Find previous line start
        int prev_line_start = find_line_start(current_line_start - 1);
        
        // Calculate current column position
        int current_column = cursor_pos - current_line_start;
        
        // Find end of previous line
        int prev_line_end = current_line_start - 1;
        if (prev_line_end < 0) prev_line_end = 0;
        
        // Calculate target position in previous line
        int prev_line_length = prev_line_end - prev_line_start;
        int target_pos = prev_line_start + current_column;
        
        // Don't go beyond previous line length
        if (target_pos > prev_line_end) {
            target_pos = prev_line_end;
        }
        
        cursor_pos = target_pos;
    }

    

    void move_cursor_down() {
        if (buffer[cursor_pos] == 0) return; // At end of buffer
        
        // Find current line start
        int current_line_start = find_line_start(cursor_pos);
        
        // Find next line start (after next newline)
        int next_line_start = cursor_pos;
        while (buffer[next_line_start] != 0 && buffer[next_line_start] != '\n') {
            next_line_start++;
        }
        if (buffer[next_line_start] == '\n') {
            next_line_start++; // Move past the newline
        } else {
            return; // No next line
        }
        
        // Calculate current column position
        int current_column = cursor_pos - current_line_start;
        
        // Find end of next line
        int next_line_end = next_line_start;
        while (buffer[next_line_end] != 0 && buffer[next_line_end] != '\n') {
            next_line_end++;
        }
        
        // Calculate target position in next line
        int target_pos = next_line_start + current_column;
        
        // Don't go beyond next line length
        if (target_pos > next_line_end) {
            target_pos = next_line_end;
        }
        
        cursor_pos = target_pos;
    }

    int find_line_start(int pos) {
        int line_start = 0;
        for (int i = 0; i < pos && buffer[i] != 0; i++) {
            if (buffer[i] == '\n') {
                line_start = i + 1;
            }
        }
        return line_start;
    }
    
    void show_message(const char* msg, unsigned char attr) {
        // Clear message area
        for (int x = 30; x < 50; x++) {
            putc_xy(x, HEIGHT - 1, ' ', 0x10);
        }
        print_string(msg, 30, HEIGHT - 1, attr);
    }

    void run() {
        draw_editor();
        while (true) {
            unsigned char scan_code = read_scan_code();
            if (scan_code == 0x01) break; // ESC to exit
            handle_input(scan_code);
        }
    }
};


// --- CommandLine class (improved formatting, safe buffers) ---
class CommandLine {
private:
    char input_buffer[100];
    int cursor_pos;
    void list_files_command() {
        RAMDiskFileEntry files[16];
        int file_count = fs_get_file_list(files, 16);
        
        if (file_count == 0) {
            show_output("No files in RAM disk", 0x47);
        } else {
            char output[80];
            copy_str(output, "Files in RAM disk:");
            show_output(output, 0x1F);
            
            for (int i = 0; i < file_count; i++) {
                char file_info[60];
                char* ptr = file_info;
                
                // Format: "filename.txt (123 bytes)"
                copy_str(ptr, "  ");
                ptr += 2;
                copy_str(ptr, files[i].filename);
                ptr += strlen(files[i].filename);
                copy_str(ptr, " (");
                ptr += 2;
                char size_str[10];
                itoa(size_str, files[i].size, 10);
                copy_str(ptr, size_str);
                ptr += strlen(size_str);
                copy_str(ptr, " bytes)");
                
                show_output(file_info, 0x17);
            }
        }
    }

    
    
    void save_file_command() {
        if (strlen(input_buffer) < 6) {
            show_output("Usage: save filename", 0x47);
            return;
        }
        
        const char* filename = input_buffer + 5;
        if (strlen(filename) == 0) {
            show_output("Usage: save filename", 0x47);
            return;
        }
        
        // Save actual content instead of hardcoded text
        const char* content = " ";
        if (fs_create_file(filename, (const u8*)content, strlen(content))) {
            char msg[50];
            copy_str(msg, "Saved: ");
            copy_str(msg + 7, filename);
            show_output(msg, 0x1E);
        } else {
            char msg[50];
            copy_str(msg, "Save failed - disk full?");
            show_output(msg, 0x47);
        }
    }
    
    void load_file_command() {
    if (strlen(input_buffer) < 6) {
        show_output("Usage: load filename", 0x47);
        return;
    }
    
    const char* filename = input_buffer + 5;
    if (strlen(filename) == 0) {
        show_output("Usage: load filename", 0x47);
        return;
    }
    
    // Check if file exists first
    if (!fs_file_exists(filename)) {
        char msg[50];
        copy_str(msg, "File not found: ");
        copy_str(msg + 16, filename);
        show_output(msg, 0x47);
        return;
    }
    
    // Launch editor with the file loaded
    TextEditor editor;
    editor.load_file(filename);  // Load file into editor
    editor.run();               // Run editor
    
    // After editor returns, redraw interface
    clear_screen(0x10);
    draw_static_interface();
    update_time_display();
}
    
    void cat_file_command() {
    // Debug: Show what we received
    char debug_msg[80];
    copy_str(debug_msg, "DEBUG input: [");
    copy_str(debug_msg + 14, input_buffer);
    copy_str(debug_msg + 14 + strlen(input_buffer), "]");
    show_output(debug_msg, 0x47);
    
    if (strlen(input_buffer) < 5) { // "cat " is 4 chars + space
        show_output("Usage: cat filename", 0x47);
        return;
    }
    
    const char* filename = input_buffer + 4; // Skip "cat "
    
    // Debug: Show the extracted filename
    char debug_fn[50];
    copy_str(debug_fn, "DEBUG filename: [");
    copy_str(debug_fn + 17, filename);
    copy_str(debug_fn + 17 + strlen(filename), "]");
    show_output(debug_fn, 0x47);
    
    if (strlen(filename) == 0) {
        show_output("Usage: cat filename", 0x47);
        return;
    }
    
    // Debug: Check if file exists first
    if (!fs_file_exists(filename)) {
        char msg[60];
        copy_str(msg, "File does not exist: ");
        copy_str(msg + 20, filename);
        show_output(msg, 0x47);
        
        // Show available files
        RAMDiskFileEntry files[16];
        int file_count = fs_get_file_list(files, 16);
        if (file_count > 0) {
            show_output("Available files:", 0x17);
            for (int i = 0; i < file_count; i++) {
                char avail_file[40];
                copy_str(avail_file, "  ");
                copy_str(avail_file + 2, files[i].filename);
                show_output(avail_file, 0x17);
            }
        }
        return;
    }
    
    u8 file_buffer[1000];
    if (fs_read_file(filename, file_buffer, sizeof(file_buffer))) {
        // Debug: Show file size
        RAMDiskFileEntry file_info;
        // We need to get file size - let's create a helper function
        u32 file_size = 0;
        // For now, just display the content
        show_output((char*)file_buffer, 0x1E);
    } else {
        char msg[60];
        copy_str(msg, "Read failed for: ");
        copy_str(msg + 17, filename);
        show_output(msg, 0x47);
    }
}
    
    void delete_file_command() {
        if (strlen(input_buffer) < 4) { // "rm " is 3 chars
            show_output("Usage: rm filename", 0x47);
            return;
        }
        
        const char* filename = input_buffer + 3;
        if (strlen(filename) == 0) {
            show_output("Usage: rm filename", 0x47);
            return;
        }
        
        if (fs_delete_file(filename)) {
            char msg[50];
            copy_str(msg, "Deleted: ");
            copy_str(msg + 9, filename);
            show_output(msg, 0x1E);
        } else {
            char msg[50];
            copy_str(msg, "Delete failed: ");
            copy_str(msg + 15, filename);
            show_output(msg, 0x47);
        }
    }
    
    // String compare for first n characters
int strncmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return s1[i] - s2[i];
        }
        if (s1[i] == 0) {
            return 0;
        }
    }
    return 0;
}

    void filesystem_stats_command() {
        u32 free_space = fs_get_free_space();
        u32 total_space = 1024 * 1024; // 1MB RAM disk
        u32 used_space = total_space - free_space;
        
        RAMDiskFileEntry files[16];
        int file_count = fs_get_file_list(files, 16);
        
        char stats[120];
        char* ptr = stats;
        
        copy_str(ptr, "RAM Disk: ");
        ptr += 10;
        char used_str[10];
        itoa(used_str, used_space / 1024, 10);
        copy_str(ptr, used_str);
        ptr += strlen(used_str);
        copy_str(ptr, "K used, ");
        ptr += 8;
        char free_str[10];
        itoa(free_str, free_space / 1024, 10);
        copy_str(ptr, free_str);
        ptr += strlen(free_str);
        copy_str(ptr, "K free, ");
        ptr += 8;
        char files_str[5];
        itoa(files_str, file_count, 10);
        copy_str(ptr, files_str);
        ptr += strlen(files_str);
        copy_str(ptr, " files");
        
        show_output(stats, 0x1E);
    }

public:
    CommandLine() : cursor_pos(0) {
        for (int i = 0; i < 100; i++) input_buffer[i] = 0;
    }

    void clear_input() {
        for (int x = 18; x < 63; x++) putc_xy(x, 23, '_', 0x17);
        cursor_pos = 0;
        for (int i = 0; i < 100; i++) input_buffer[i] = 0;
        putc_xy(17, 23, '>', 0x2F);
    }

    void display_input() {
        for (int x = 18; x < 63; x++) putc_xy(x, 23, ' ', 0x17);
        for (int i = 0; i < cursor_pos && i < 45; i++) putc_xy(18 + i, 23, input_buffer[i], 0x17);
        // cursor
        putc_xy(18 + cursor_pos, 23, '_', 0x4F);
    }

    void handle_input(unsigned char scan_code) {
        char ascii = scan_code_to_ascii(scan_code);
        if (scan_code == 0x01) {
            // ESC - handled by outer loop (we return to launch editor)
            return;
        } else if (scan_code == 0x0E) {
            if (cursor_pos > 0) {
                cursor_pos--;
                input_buffer[cursor_pos] = 0;
            }
        } else if (scan_code == 0x1C) {
            execute_command();
            clear_input();
        } else if (ascii != 0 && cursor_pos < 99) {
            input_buffer[cursor_pos++] = ascii;
            input_buffer[cursor_pos] = 0;
        }
        display_input();
    }

    void show_output(const char* text, unsigned char attr = 0x17) {
        // clear area
        for (int y = 19; y <= 21; y++) for (int x = 16; x < 64; x++) putc_xy(x, y, ' ', 0x10);
        print_centered(text, 20, attr);
    }


    void execute_command() {
    if (input_buffer[0] == 0) return;

    if (strcmp(input_buffer, "help") == 0) {
        show_output_wrapped("COMMANDS: help, clear, about, status, time, date, mem, meminfo, mmap, alloc, ls, save, load, cat, rm", 0x1F);
    } else if (strcmp(input_buffer, "clear") == 0) {
        show_output("OUTPUT CLEARED", 0x1E);
    } else if (strcmp(input_buffer, "about") == 0) {
        show_output("ATOMIC OS v0.1 - 32BIT KERNEL WITH MEMORY MANAGEMENT", 0x1F);
    } else if (strcmp(input_buffer, "status") == 0) {
        show_output("SYSTEM STATUS: OPTIMAL", 0x1E);
    } else if (strcmp(input_buffer, "time") == 0) {
        Time now = read_rtc_time();
        char t[9]; format_time(t, now);
        char out[32] = "TIME: ";
        copy_str(out + 6, t);
        show_output(out, 0x1E);
    } else if (strcmp(input_buffer, "date") == 0) {
        Time now = read_rtc_time();
        char d[11]; format_date(d, now);
        char out[32] = "DATE: ";
        copy_str(out + 6, d);
        show_output(out, 0x1E);
    }
    
     else if (strcmp(input_buffer, "ls") == 0 || strcmp(input_buffer, "dir") == 0) {
        list_files_command();
    } else if (strncmp(input_buffer, "save ", 5) == 0) {
    save_file_command();
} else if (strncmp(input_buffer, "load ", 5) == 0) {
    load_file_command();
} else if (strncmp(input_buffer, "cat ", 4) == 0) {
    cat_file_command();
} else if (strncmp(input_buffer, "rm ", 3) == 0) {
    delete_file_command();
}

    else if (strcmp(input_buffer, "mem") == 0) {
        u32 used = g_allocator.get_used_memory();
        u32 total = g_allocator.get_total_memory();
        char mem_info[50];
        
        char used_str[10];
        char total_str[10];
        
        itoa(used_str, used / 1024, 10);
        itoa(total_str, total / 1024, 10);
        
        char* ptr = mem_info;
        copy_str(ptr, "MEM: ");
        ptr += 5;
        copy_str(ptr, used_str);
        ptr += strlen(used_str);
        copy_str(ptr, "K/");
        ptr += 2;
        copy_str(ptr, total_str);
        ptr += strlen(total_str);
        copy_str(ptr, "K USED");
        
        show_output(mem_info, 0x1E);
    } else if (strcmp(input_buffer, "meminfo") == 0) {
        u32 used = g_allocator.get_used_memory();
        u32 total_alloc = g_allocator.get_total_memory();
        u32 total_system = get_total_usable_memory();
        
        char info[80];
        char* ptr = info;
        
        copy_str(ptr, "ALLOC: ");
        ptr += 7;
        char used_str[10];
        itoa(used_str, used / 1024, 10);
        copy_str(ptr, used_str);
        ptr += strlen(used_str);
        copy_str(ptr, "K/");
        ptr += 2;
        char alloc_str[10];
        itoa(alloc_str, total_alloc / 1024, 10);
        copy_str(ptr, alloc_str);
        ptr += strlen(alloc_str);
        copy_str(ptr, "K  SYSTEM: ");
        ptr += 11;
        char sys_str[10];
        itoa(sys_str, total_system / (1024*1024), 10);
        copy_str(ptr, sys_str);
        ptr += strlen(sys_str);
        copy_str(ptr, "MB RAM");
        
        show_output(info, 0x1E);
    } 
    
    else if (strcmp(input_buffer, "mmap") == 0) {
        char mmap_info[80];
        char* ptr = mmap_info;
        
        copy_str(ptr, "MEMORY MAP: ");
        ptr += 12;
        char entries_str[10];
        itoa(entries_str, memory_map_entries, 10);
        copy_str(ptr, entries_str);
        ptr += strlen(entries_str);
        copy_str(ptr, " REGIONS DETECTED");
        
        show_output(mmap_info, 0x1E);
        
        // Show first few regions
        int regions_to_show = (memory_map_entries < 3) ? memory_map_entries : 3;
        for (int i = 0; i < regions_to_show; i++) {
            char region_info[60];
            char* rptr = region_info;
            
            // Type string
            const char* type_str = "UNKNOWN";
            switch(memory_map[i].type) {
                case MEMORY_AVAILABLE: type_str = "AVAIL"; break;
                case MEMORY_RESERVED: type_str = "RSRVD"; break;
                default: type_str = "OTHER"; break;
            }
            
            copy_str(rptr, type_str);
            rptr += 5;
            copy_str(rptr, " 0x");
            rptr += 3;
            
            // Convert address to hex string (simplified)
            u32 addr = (u32)memory_map[i].base_addr;
            for (int j = 0; j < 8; j++) {
                int digit = (addr >> (28 - j*4)) & 0xF;
                *rptr++ = (digit < 10) ? '0' + digit : 'A' + digit - 10;
            }
            
            copy_str(rptr, "-");
            rptr++;
            
            u32 end_addr = (u32)(memory_map[i].base_addr + memory_map[i].length);
            for (int j = 0; j < 8; j++) {
                int digit = (end_addr >> (28 - j*4)) & 0xF;
                *rptr++ = (digit < 10) ? '0' + digit : 'A' + digit - 10;
            }
            
            *rptr = 0;
            show_output(region_info, 0x17);
        }
    } else if (strcmp(input_buffer, "alloc") == 0) {
        void* test_ptr = kmalloc(1024);
        if (test_ptr) {
            show_output("ALLOCATED 1KB - TEST PASSED", 0x1E);
        } else {
            show_output("ALLOCATION FAILED", 0x47);
        }
    } 
    else {
        show_output("UNKNOWN COMMAND - TYPE 'help'", 0x47);
    }
}
};


// --- main ---
extern "C" void main() {
    initialize_memory();
    fs_initialize(); 
    // draw whole static interface once
    clear_screen(0x10);
    draw_static_interface();
    update_time_display();

    CommandLine cmd;

    while (true) {
        // Refresh time in header and holographic box frequently (but not full redraw)
        update_time_display();

        cmd.clear_input();
        cmd.show_output("SYSTEM INITIALIZED - AWAITING INPUT", 0x1E);

        // listen for keyboard events for the CLI
        while (true) {
            unsigned char scan_code = read_scan_code();

            // ESC launches editor
            if (scan_code == 0x01) {
                TextEditor editor;
                editor.run();
                // After editor returns, redraw the static UI (editor cleared screen)
                clear_screen(0x10);
                draw_static_interface();
                update_time_display();
                break;
            }

            cmd.handle_input(scan_code);
            // keep header time fresh while typing
            // (In real kernel you'd update based on timer interrupts to save power)
            update_time_display();
        }
    }
}