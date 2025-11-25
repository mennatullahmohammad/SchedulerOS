#include "headers.h"

#define MAX_PROCESSES 100
#define MSG_KEY 65   


struct Node* ready_queue = NULL; 
struct PCB* current_process = NULL; 

struct PCB all_processes[MAX_PROCESSES]; //keep track of all processes
int process_count = 0;

int msgid;


// Updates the master list's PID from Virtual ID to OS PID (Called after fork)
void update_master_list_pid(int virt_id, int os_pid, int start_time) {
    for(int i = 0; i < process_count; i++) {
        if(all_processes[i].P.PID == virt_id) {
            all_processes[i].P.PID = os_pid; 
            all_processes[i].start_time = start_time;
            break;
        }
    }
}

// Updates the master list when a process finishes
void update_master_list_finish(int pid, int time) {
    for(int i = 0; i < process_count; i++) {
        if(all_processes[i].P.PID == pid) {
            all_processes[i].finish_time = time;
            all_processes[i].state = FINISHED;
            all_processes[i].P.RemainingTime = 0;
            break;
        }
    }
}

// Implement process start: fork + exec (give child its params).

void start_new_process(struct PCB* p, int current_time) {
    char str_runtime[10];  //command for execl must be string
    sprintf(str_runtime, "%d", p->P.Runtime); //converts runtime integer to string
    
    // process generator id to find the PCB in master list later
    int proc_genID = p->P.PID; 

    pid_t pid = fork();
    
    if (pid == 0) {   //child, it executes process code 
        execl("./process.out", "process.out", str_runtime, NULL); //replaces child with code from process.c
        exit(1); //normal exit
    }
    
    //parent update PCB PID and state
    p->P.PID = pid;
    p->start_time = current_time;
    p->state = RUNNING;
    
    update_master_list_pid(proc_genID, pid, current_time);
}

//Implement process switch: stop old process (SIGSTOP), save state (PCB fields), resume new (SIGCONT).
void switch_process(struct PCB* old_proc, struct PCB* new_proc, int current_time) {
    // send sig stop old process (preemption)
    kill(old_proc->P.PID, SIGSTOP);
    
    //its state is ready not running
    old_proc->state = READY;
    
    //enqueue  to ready queue
    enqueue(&ready_queue, *old_proc, ALG_SRTN);

   
    // start_time is -1, it has never run before so start 
    if (new_proc->start_time == -1) {
         start_new_process(new_proc, current_time);
    } else {
         //resume
         kill(new_proc->P.PID, SIGCONT);
         new_proc->state = RUNNING;
    }
}

//Implement Shortest Remaining Time Next (SRTN) (preemptive shortest remaining time).

void SRTN_scheduler(int current_time) {
    struct PCB* shortest_job = peek(ready_queue);

    // case 1 CPU is Idle ,dequeue
    if (current_process->P.PID == -1 && shortest_job != NULL) { //no current process
        dequeue(&ready_queue, current_process); 
        
        if (current_process->start_time == -1) {
            start_new_process(current_process, current_time);
        } else {
            kill(current_process->P.PID, SIGCONT);
            current_process->state = RUNNING;
        }
    }
    
    // case 2 CPU is Busy, but queue has a shorter job
    else if (current_process->P.PID != -1 && shortest_job != NULL) {
        if (shortest_job->P.RemainingTime < current_process->P.RemainingTime) {
            
            // swap processes. 
            struct PCB next_proc;
            dequeue(&ready_queue, &next_proc); 
            
            
            switch_process(current_process, &next_proc, current_time);
            
            //current is next 
            *current_process = next_proc;
        }
    }
}

int main(int argc, char * argv[])
{
    initClk(); 
    
    // ftok to match process gen
    key_t key_id = ftok("keyfile", 65);
    msgid = msgget(key_id, 0666 | IPC_CREAT);

    //recieve algorithm
    struct AlgorithmMsg alg_msg;
    msgrcv(msgid, &alg_msg, sizeof(alg_msg.mtext), 1, 0);

    //allocate memory for PCB
    current_process = malloc(sizeof(struct PCB));
    current_process->P.PID = -1; // -1 means CPU is Idle
    current_process->start_time = -1;

    int prev_time = -1;

    while (1) {
        int clock_time = getClk();
        if (clock_time == prev_time) continue; //if clk somehow didnt increase, skip
        prev_time = clock_time;

        // check for finished processes
        int status;
        pid_t finished_pid;
        while ((finished_pid = waitpid(-1, &status, WNOHANG)) > 0) { //wait for child to finish,if no one finished dont block
            update_master_list_finish(finished_pid, clock_time);
            
            // if finished process was running on CPU
            if (current_process->P.PID != -1 && current_process->P.PID == finished_pid) {
                current_process->P.RemainingTime = 0;
                current_process->state = FINISHED;
                current_process->P.PID = -1; //CPU is now Idle
            }
        }

        //update remaining time
        if (current_process->P.PID != -1) {
            current_process->P.RemainingTime--;
        }

        //receive new processes & Create PCB
        struct ProcessMsg proc_msg; //mtype=2
        while (msgrcv(msgid, &proc_msg, sizeof(proc_msg.process), 2, IPC_NOWAIT) != -1) { //if message recieves, then !=-1
            struct PCB new_proc;
            new_proc = proc_msg.process;
            
            //initalise PCB state
            new_proc.state = READY;
            new_proc.start_time = -1;
            
            //add to all processes
            all_processes[process_count++] = new_proc;
            
            //add to ready queue
            if (strcmp(alg_msg.mtext, "SRTN") == 0) {
                enqueue(&ready_queue, new_proc, ALG_SRTN);
            }
            else if (strcmp(alg_msg.mtext, "HPF") == 0) {
                // Enqueue logic for HPF
            }
            else if (strcmp(alg_msg.mtext, "RR") == 0) {
                // Enqueue logic for RR
            }
        }

        //if no new process, just keep running agorithm
        if (strcmp(alg_msg.mtext, "SRTN") == 0) {
            SRTN_scheduler(clock_time);
        }
        else if (strcmp(alg_msg.mtext, "HPF") == 0) {
            // HPF Scheduler Logic
        }
        else if (strcmp(alg_msg.mtext, "RR") == 0) {
            // RR Scheduler Logic
        }

        //check if all finished
        int all_done = 1;
        if (process_count > 0) {
            for(int i = 0; i < process_count; i++) {
                if (all_processes[i].state != FINISHED) {
                    all_done = 0;
                    break;
                }
            }
            if (all_done && ready_queue == NULL && current_process->P.PID == -1) {
                break;
            }
        }
    }

    msgctl(msgid, IPC_RMID, NULL);//destroy message queue
    destroyClk(true);
    return 0;
}

    //TODO implement the scheduler :)
    //upon termination release the clock resources.



    
    //ready queue -> queues from ds
    //1-Implement Shortest Remaining Time Next (SRTN) (preemptive shortest remaining time). (DONE)

    //2- Implement process start: fork + exec (give child its params). (DONE)

    //3- Implement process switch: stop old process (SIGSTOP), save state (PCB fields), resume new (SIGCONT). (DONE)
    //take input to see which process to run

    //4- Maintain PCB for each process (state, remaining, arrival, start/finish times, pid, wait times, WTA/TA).(DONE)

    //5- Maintain ready queue optimized for SRTN.

    //6-React to process arrivals (from process_generator) and process termination notifications (from children / process).

    //7-Produce Scheduler.log (events) and Scheduler.perf (final metrics).

    //8-Clean up IPC on exit.
  


    /*
    * Implement the Shortest Remaining Time Next (SRTN) scheduling algorithm.
    * Implement the logic to switch between two processes (stop the old, save its state, start/resume another).
    * Implement the logic to start a new process (fork it and give it its parameters).
    * Manage the Process Control Block (PCB) for each process, keeping track of its state (running/waiting), remaining time, etc..
    * Define and manage the ready queue data structure for the SRTN algorithm.
    */

    
