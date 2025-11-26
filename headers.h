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
int * shmaddr;                 //
//===============================



int getClk()
{
    return *shmaddr;
}


/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
*/
void initClk()
{
    int shmid = shmget(SHKEY, 4, 0444);
    while ((int)shmid == -1)
    {
        //Make sure that the clock exists
        printf("Wait! The clock not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
    }
    shmaddr = (int *) shmat(shmid, (void *)0, 0);
}


/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
*/

void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}


//Process struct

struct Process {
    pid_t PID;
    int ArrivalTime; 
    int Runtime;   
    int Priority;
    int DependencyID;
    int RemainingTime;
    int expri; //for pri inheritance
};


//enum for process state
typedef enum {READY, RUNNING, FINISHED, BLOCKED} proc_state;

//pcb struct
struct PCB {
    struct Process P;
    int start_time;    
    int finish_time;   
    int last_start_time;
    int waiting_time; 
    int turnaround; 
    double Wta;
    proc_state state;  //one of the enums 0,1,2
    bool depp; //does it have others depending on it?
    int blockedID; //process blocked by it (-1 when nonexistant)
};

//Linked List DS definition

struct Node {    
    struct PCB Entry;
    struct Node* next;
};

void printLinkedlist(struct Node *p) {
  while (p != NULL) {
    printf("%d ", p->Entry.P.PID);
    p = p->next;
  }
}

//MQ Structs

struct AlgorithmMsg {
    long mtype;         
    char mtext[20];       
};

struct ProcessMsg {
    long mtype;         
    struct PCB process;
};

typedef enum { ALG_SRTN, ALG_HPF } Algorithm;

bool enqueue(struct Node** head, struct PCB *p, Algorithm alg)
{
    struct Node* newNode = malloc(sizeof(struct Node));
    if (!newNode) return false;

    newNode->Entry = *p;
    newNode->next = NULL;

    // Case 1: empty list
    if (*head == NULL)
    {
        *head = newNode;
        return true;
    }

    // Decide comparison depending on algorithm
    struct Node* cur = *head;

    // Case 2: new node should be head
    if ((alg == ALG_SRTN  && p->P.RemainingTime < cur->Entry.P.RemainingTime) ||
        (alg == ALG_HPF   && p->P.Priority > cur->Entry.P.Priority))
    {
        newNode->next = *head;
        *head = newNode;
        return true;
    }

    // Case 3: middle insertion
    while (cur->next != NULL &&
          ((alg == ALG_SRTN && cur->next->Entry.P.RemainingTime <= p->P.RemainingTime) ||
           (alg == ALG_HPF  && cur->next->Entry.P.Priority      >= p->P.Priority)))
    {
        cur = cur->next;
    }

    newNode->next = cur->next;
    cur->next = newNode;
    return true;
}



bool dequeue(struct Node** head, struct PCB* out)   
{
    if (*head == NULL) return false;

    struct Node* temp = *head;
    *out = temp->Entry;

    *head = (*head)->next;
    free(temp);
    return true;
}

struct PCB* peek(struct Node* head) {
    if (head == NULL) return NULL;
    return &(head->Entry);
}

//ROUND ROBIN CIRCULAR QUEUE IMPLEMENTATION
void enqueueRR(struct Node** head, struct Node** tail, struct PCB p) {
    struct Node* newNode = malloc(sizeof(struct Node));
    newNode->Entry = p;
    newNode->next = NULL;

    if (*head == NULL) {
        *head = *tail = newNode;
    } else {
        (*tail)->next = newNode;
        *tail = newNode;
    }
}


struct PCB dequeueRR(struct Node** head, struct Node** tail) {
    struct PCB p = (*head)->Entry;
    struct Node* temp = *head;
    *head = (*head)->next;
    if (*head == NULL) 
        *tail = NULL; // queue empty
    free(temp);
    return p;
}


void moveToTail(struct Node* node, struct Node* tail) {
    // Only call if node ran less than its runtime, ya3ny lw lesa el runtime makhles4 move it
    if (tail) 
        tail->next = node;
    tail = node;
    node->next = NULL;
}


#endif