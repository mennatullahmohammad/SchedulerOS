#ifndef MMU_H
#define MMU_H

#define MEMORY_SIZE 512          // Total RAM size in bytes
#define PAGE_SIZE 16             // Page size in bytes
#define FRAME_COUNT (MEMORY_SIZE / PAGE_SIZE)  // 32 frames


typedef struct Frame {
    int free;        // 1 = frame is free, 0 = not free
    int pid;         // Process ID that owns this frame
    int vpn;         // Virtual Page Number stored in this frame
    int ref;         // Reference bit 
    int modified;    // 1 if page was written 
};



typedef struct PageTableEntry {
    int valid;       // 1 = page is in memory
    int frame;       // Physical frame number
    int ref;         // Reference bit
    int modified;    // Modified bit
};


extern struct Frame frame_table[FRAME_COUNT];


  // MMU Helper Functions
 

void init_memory(void);
int find_free_frame(void);

#endif