#include "MMU.h"


struct Frame frame_table[FRAME_COUNT]; //phy memory ->RAM



void init_memory(void) {
    for (int i = 0; i < FRAME_COUNT; i++) {
        frame_table[i].free = 1;     // Frame is free ,kol frames btbda2 free 
        frame_table[i].pid = -1;     
        frame_table[i].vpn = -1;     // No virtual page
        frame_table[i].ref = 0;      
        frame_table[i].modified = 0; 
    }
}


int find_free_frame(void) {
    for (int i = 0; i < FRAME_COUNT; i++) {
        if (frame_table[i].free == 1) {
            return i;
        }
    }
    return -1;  // No free frame
}