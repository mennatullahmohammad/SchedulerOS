#include "headers.h"

void clearResources(int);
int queue_id;


int main(int argc, char * argv[])
{
    FILE *file_ptr;
    char buffer[256]; 
    file_ptr = fopen("processes.txt", "r");

    if (file_ptr == NULL) {
        printf("Error: Could not open file.\n");
        return 1; // Indicate an error
    }

    struct NodePCB* Head =  NULL;

    while(fgets(buffer, sizeof(buffer), file_ptr)) 
    {
        struct NodePCB* temp = malloc(sizeof(struct NodePCB));
        temp->next = NULL;

        sscanf(buffer, "%d %d %d %d %d",
           &temp->P.PID,
           &temp->P.ArrivalTime,
           &temp->P.Runtime,
           &temp->P.Priority,
           &temp->P.DependencyID);
        
        temp->P.RemainingTime = temp->P.Runtime;

        if (Head == NULL) {
        Head = temp;
        } 
        else {
        struct NodePCB* curr = Head;
        while (curr->next != NULL) curr = curr->next;
        curr->next = temp;
        }
    }  

    fclose(file_ptr);

    signal(SIGINT, clearResources);
    // TODO Initialization
    // 1. Read the input files. DONE
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any. DONE
    // 3. Initiate and create the scheduler and clock processes. DONE
    // 4. Use this function after creating the clock process to initialize clock DONE

    key_t queue_key = ftok("keyfile", 65);
        
    if (queue_key == -1)
        {
            printf("error in creating message queue key");
            exit(1);
        
        }
    queue_id = msgget(queue_key, 0666 | IPC_CREAT);
    if (queue_id == -1) 
    {
        perror("Error In Creating message queue.");
        exit(1);
    }

    struct AlgorithmMsg alg;
    struct ProcessMsg proc;
    pid_t pid = getpid();

    
    pid_t clk = fork();
    if (clk == 0)
    {
        execl("./scheduler.out", "scheduler.out", NULL);
        perror("execl failed");  // in case exec fails
        exit(1);
    }
    else
    {
        printf("Which Algorithm do you want to use? (HPF, STRN, RR)");

        fgets(alg.mtext, 20, stdin);

        alg.mtext[strcspn(alg.mtext, "\n")] = '\0';
        alg.mtype = 1;
        if (msgsnd(queue_id, &alg, sizeof(alg.mtext), 0) == -1) 
        {
            perror("Error In Sending algorithm to scheduler.");
            exit(1);
        }

        initClk();

        while (Head != NULL) 
        {
            while (getClk() < Head->P.ArrivalTime) 
            {
                ; // busy wait
            }

            proc.mtype = 2;
            proc.process = Head->P;
            if (msgsnd(queue_id, &proc, sizeof(proc.process), 0) == -1) 
            {
                perror("Error sending process to scheduler");
                exit(1);
            }

            struct NodePCB* tmp = Head;
            Head = Head->next;
            free(tmp);
        }       
    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters. DONE
    // 6. Send the information to the scheduler at the appropriate time.
    // 7. Clear clock resources 
    }
}

void clearResources(int signum)
{
    //TODO Clears all resources in case of interruption DONE
    msgctl(queue_id, IPC_RMID, NULL);
    destroyClk(true);
    exit(0);
}
