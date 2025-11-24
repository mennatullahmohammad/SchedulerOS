
#include "headers.h"
#define true 1
#define false 0

/////enqueue and dequeue priority queue for strn

struct Node {    
    struct PCB P;
    struct Node* next;
};

bool enqueueSTRN(struct Node** head, struct PCB p)
{
    struct Node* newNode = malloc(sizeof(struct Node));  //dynamically allocate space to node
    if (!newNode) return false;

    newNode->P = p;
    newNode->next = NULL;

    if (*head == NULL || p.remaining_time < (*head)->P.remaining_time)  //if process remaining time is shortest than                                                                 
    {                                                                 //head or no head
        newNode->next = *head;
        *head = newNode;
        return true;
    }

    struct Node* cur = *head;
    while (cur->next != NULL && cur->next->P.remaining_time <= p.remaining_time)  //loop until remainig time is less than next
        cur = cur->next;

    newNode->next = cur->next;
    cur->next = newNode;
    return true;
}

bool dequeue(struct Node** head, struct PCB* out)   
{
    if (*head == NULL) return false;

    struct Node* temp = *head;
    *out = temp->P;

    *head = (*head)->next;
    free(temp);
    return true;
}

struct PCB* peek(struct Node* head) {
    if (head == NULL) return NULL;
    return &(head->P);
}

