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

    int process_count = 0;

    while (fgets(buffer, sizeof(buffer), file_ptr)) {
        if (buffer[0] != '#') { // Skip comment lines
            process_count++;
        }
    }

    rewind(file_ptr);

    // Allocate memory for processes
    struct Process *processes = malloc(process_count * sizeof(struct Process));
    if (processes == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    int index = 0;

    while(fgets(buffer, sizeof(buffer), file_ptr)) 
    {
       if(buffer[0] != '#')
        {sscanf(buffer, "%d %d %d %d %d",
            &processes[index].PID,           
            &processes[index].ArrivalTime,   
            &processes[index].Runtime,       
            &processes[index].Priority,      
            &processes[index].DependencyID); 
            index++;
        }
    }  

    fclose(file_ptr);

    signal(SIGINT, clearResources);
    // TODO Initialization
    // 1. Read the input files. DONE
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any. DONE
    // 3. Initiate and create the scheduler and clock processes. DONE
    // 4. Use this function after creating the clock process to initialize clock DONE

    key_t queue_key = ftok("/tmp", 'M');
        
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
    struct DoneMsg done;

        printf("Which Algorithm do you want to use? (HPF, SRTN, RR)");

        fgets(alg.mtext, 20, stdin);

        alg.mtext[strcspn(alg.mtext, "\n")] = '\0';
        alg.mtype = 1;
        if (msgsnd(queue_id, &alg, sizeof(alg.mtext), 0) == -1) 
        {
            perror("Error In Sending algorithm to scheduler.");
            exit(1);
        }

        pid_t clock_pid = fork();
        if (clock_pid == 0) {
        execl("./clk.out", "clk.out", NULL);
        perror("Error starting clock");
        exit(EXIT_FAILURE);
        }

    initClk();
    
    pid_t sched = fork();
    if (sched == 0)
    {
        execl("./scheduler.out", "scheduler.out", NULL);
        perror("execl failed");  // in case exec fails
        exit(1);
    }
    else
    {

        int sent_processes = 0;
        int next_process_index = 0;

        while (sent_processes < process_count) 
        {
            while (getClk() < processes[next_process_index].ArrivalTime) 
            {
                ; // busy wait
            }

            proc.mtype = 2;
            proc.proc = processes[next_process_index];
            if (msgsnd(queue_id, &proc, sizeof(proc.proc), 0) == -1) 
            {
                perror("Error sending process to scheduler");
                exit(1);
            }
            sent_processes++;
            next_process_index++;

        }      
        
        done.mtype = 3; // Message type for end of processes
        done.dummy[0] = 0;
        if (msgsnd(queue_id, &done, sizeof(done.dummy), 0) == -1) {
        perror("Error sending generator done message");
        exit(1);
        }
        // Wait for scheduler to finish
        waitpid(sched, NULL, 0);
    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters. DONE
    // 6. Send the information to the scheduler at the appropriate time.
    // 7. Clear clock resources 
    }

    free(processes);
    msgctl(queue_id, IPC_RMID, NULL);
    destroyClk(true);
    return 0;
}

void clearResources(int signum)
{
    //TODO Clears all resources in case of interruption DONE
    msgctl(queue_id, IPC_RMID, NULL);
    killpg(getpgrp(), SIGKILL);
    destroyClk(true);
    exit(0);
}
