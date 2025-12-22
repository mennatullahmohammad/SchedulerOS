#ifndef HEADERS_H
#define HEADERS_H

#include <stdio.h>      //if you don't use scanf/printf change this include
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#define MAX_PROCESSES 100
#define MSG_KEY 65


typedef short bool;
#define true 1
#define false 0

#define SHKEY 300


///==============================
//don't mess with this variable//
extern int * shmaddr;                 //
//===============================



int getClk();


/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
*/
void initClk();


/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
*/

void destroyClk(bool terminateAll);


//Process struct

struct Process {
    pid_t PID;
    int ArrivalTime; 
    int Runtime;   
    int Priority;
    int DependencyID;
    int disk_base;           
    int disk_limit;         
};


//enum for process state
typedef enum {READY, RUNNING, FINISHED, BLOCKED} proc_state;

//pcb struct
struct PCB {
    int virtual_pid;   // original ID from processes.txt
    struct Process P;
    int RemainingTime;
    int expri; //for pri inheritance
    int start_time;    
    int finish_time;   
    int last_start_time;
    int waiting_time; 
    int turnaround; 
    double Wta;
    proc_state state;  //one of the enums
    bool depp; //does it have others depending on it?
    int blockedID[200]; //process blocked by it (-1 when nonexistant)
    int count;
    int blocked;
    int unblock_time;
    struct PageTableEntry *page_table;
    int page_table_frame;    // physical frame of page table
    FILE* req_file;          // requests.txt file pointer
};

//Linked List DS definition

struct Node {    
    struct PCB Entry;
    struct Node* next;
};

//MQ Structs

struct AlgorithmMsg {
    long mtype;         
    char mtext[20];       
};

struct ProcessMsg {
    long mtype;         
    struct Process proc;
};

struct DoneMsg {
    long mtype;  // 
    char dummy[1]; // Just a placeholder
};

struct FinishMsg {
    long mtype;           // message type = 4
    pid_t pid;            // process PID
    int finish_time;      // time when finished
};

typedef enum { ALG_SRTN, ALG_HPF } Algorithm;

bool enqueue(struct Node** head, struct PCB *p, Algorithm alg);
bool dequeue(struct Node** head, struct PCB* out);
struct PCB* peek(struct Node* head);
void enqueueRR(struct Node** head, struct Node** tail, struct PCB p);
struct PCB dequeueRR(struct Node** head, struct Node** tail);

#endif