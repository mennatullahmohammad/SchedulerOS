#include "headers.h"
int * shmaddr;


int getClk()
{
    return *shmaddr;
}

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

void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}

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
    if ((alg == ALG_SRTN  && p->RemainingTime < cur->Entry.RemainingTime) ||
        (alg == ALG_HPF   && p->P.Priority > cur->Entry.P.Priority))
    {
        newNode->next = *head;
        *head = newNode;
        return true;
    }

    // Case 3: middle insertion
    while (cur->next != NULL &&
          ((alg == ALG_SRTN && cur->next->Entry.RemainingTime <= p->RemainingTime) ||
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
    if (*head == NULL) {
        struct PCB empty={0}; // queue empty
        return empty;
    }

    struct PCB p = (*head)->Entry;
    struct Node* temp = *head;
    
    // Only one node left
    if (*head == *tail) {
        *head = NULL;
        *tail = NULL;
        free(temp);
        return p;
    }
    
    // multiple elements
    *head = (*head)->next;     // move head
    (*tail)->next = *head;     // maintain circular
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
