#include "headers.h"

#define MAX_PROCESSES 100
#define MSG_KEY 65   


struct Node* ready_queue = NULL; 
struct PCB* current_process = NULL; 
struct Node* RR_head = NULL;       // circular RR queue head (headers.h Node)
struct Node* RR_tail = NULL;       // circular RR queue tail

struct PCB all_processes[MAX_PROCESSES]; //keep track of all processes
int process_count = 0;

FILE* logFile = NULL;
int msgid;
int quantum = 2;

//getter for PCB
struct PCB* getPCB(int id){
    for(int i=0; i<process_count ; i++)
    {
        if(all_processes[i].P.PID == id)
        {
            return &all_processes[i];
        }
    }
    return NULL;
}

/* Logging helper  */
void printSchedulerLogFile(struct PCB* p, const char* state) {
    if (!logFile || !p) return;
    int t = getClk();
    int printed_pid = p->P.PID;

    if (strcmp(state, "started") == 0) {
        fprintf(logFile, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                t, printed_pid, p->P.ArrivalTime, p->P.Runtime, p->P.RemainingTime, p->waiting_time);
    } else if (strcmp(state, "resumed") == 0) {
        fprintf(logFile, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                t, printed_pid, p->P.ArrivalTime, p->P.Runtime, p->P.RemainingTime, p->waiting_time);
    } else if (strcmp(state, "stopped") == 0) {
        fprintf(logFile, "At time %d process %d stopped arr %d total %d remain %d\n",
                t, printed_pid, p->P.ArrivalTime, p->P.Runtime, p->P.RemainingTime);
    } else if (strcmp(state, "finished") == 0) {
        int TA = p->finish_time - p->P.ArrivalTime;
        if (TA < 0) TA = getClk() - p->P.ArrivalTime;
        double WTA = (p->P.Runtime > 0) ? (double)TA / (double)p->P.Runtime : 0.0;
        int wait = TA - p->P.Runtime;
        fprintf(logFile, "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n",
                getClk(), printed_pid, p->P.ArrivalTime, p->P.Runtime, wait, TA, WTA);
    }
    fflush(logFile);
}

// Updates the all list PID from id recieved in process gen to OS PID
void update_alllist_pid(int virt_id, int os_pid, int start_time) {
    for(int i = 0; i < process_count; i++) {
        if(all_processes[i].P.PID == virt_id) {
            all_processes[i].P.PID = os_pid; 
            all_processes[i].start_time = start_time;
            break;
        }
    }
}

// Updates the all list when a process finishes
void update_alllist_finish(int pid, int time) {
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
    
    update_alllist_pid(proc_genID, pid, current_time);
}

//Implement process switch: stop old process (SIGSTOP), save state (PCB fields), resume new (SIGCONT).
void switch_process(struct PCB* old_proc, struct PCB* new_proc, int current_time, Algorithm alg) {
    // send sig stop old process (preemption)
    kill(old_proc->P.PID, SIGSTOP);
    
    //its state is ready not running
    old_proc->state = READY;
    
    
    //enqueue  to ready queue
    enqueue(&ready_queue, old_proc, alg);

   
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
            
            
            switch_process(current_process, &next_proc, current_time,ALG_SRTN);
            
            //current is next 
            *current_process = next_proc;
        }
    }
}

void RR_scheduler(int current_time) {
    /* If CPU idle -> take next from RR queue  */
    if (current_process->P.PID == -1) {
        if (RR_head != NULL) {
            /* dequeue by value (your header's dequeueRR returns a struct PCB) */
            struct PCB p = dequeueRR(&RR_head, &RR_tail);
            *current_process = p;

            if (current_process->start_time == -1) {
                /* first time start */
                start_new_process(current_process, current_time);
                current_process->waiting_time = current_time - current_process->P.ArrivalTime;
                printSchedulerLogFile(current_process, "started");
            } else {
               
                if (current_process->P.PID != -1) {
                    if (kill(current_process->P.PID, SIGCONT) == -1) {
                       
                        start_new_process(current_process, current_time);
                        current_process->waiting_time = current_time - current_process->P.ArrivalTime;
                        printSchedulerLogFile(current_process, "started");
                    } else {
                        current_process->state = RUNNING;
                        current_process->last_start_time = current_time;
                        printSchedulerLogFile(current_process, "resumed");
                    }
                } else {
                    
                    start_new_process(current_process, current_time);
                    current_process->waiting_time = current_time - current_process->P.ArrivalTime;
                    printSchedulerLogFile(current_process, "started");
                }
            }

            /* set slice end time using current remaining */
            int rem = current_process->P.RemainingTime;
            if (rem <= 0) rem = current_process->P.Runtime; /* defensive */
            current_process->start_time = current_time;
            current_process->finish_time = current_time + ((rem >= quantum) ? quantum : rem);
        }
        return;
    }

        if (current_process->P.PID != -1 && current_process->state == RUNNING && getClk() >= current_process->finish_time) {
        pid_t pid = current_process->P.PID;
        /* Decrementing of RemainingTime is done in main loop per tick; here we just preempt based on slice end */

        /* If process still alive, stop and enqueue a copy */
        if (pid != -1) {
            /* Log stopped */
            printSchedulerLogFile(current_process, "stopped");

            /* Stop the child; if it exited between checks, kill will fail but we continue */
            if (kill(pid, SIGSTOP) == -1) {
                /* ignore error; child may have exited */
            }

            /* Enqueue a copy (circular queue implementation in headers.h expects a struct PCB by value) */
            struct PCB copyPCB = *current_process;
            copyPCB.state = READY;
            enqueueRR(&RR_head, &RR_tail, copyPCB);

            /* Mark CPU idle (the stopped child still exists but CPU has no active process in placeholder) */
            current_process->P.PID = -1;
            current_process->state = READY;
        }

        /*  pick next process if available */
        if (RR_head != NULL) {
            struct PCB p = dequeueRR(&RR_head, &RR_tail);
            *current_process = p;

            if (current_process->start_time == -1) {
                start_new_process(current_process, getClk());
                current_process->waiting_time = getClk() - current_process->P.ArrivalTime;
                printSchedulerLogFile(current_process, "started");
            } else {
                if (current_process->P.PID != -1) {
                    if (kill(current_process->P.PID, SIGCONT) == -1) {
                        start_new_process(current_process, getClk());
                        current_process->waiting_time = getClk() - current_process->P.ArrivalTime;
                        printSchedulerLogFile(current_process, "started");
                    } else {
                        current_process->state = RUNNING;
                        current_process->last_start_time = getClk();
                        printSchedulerLogFile(current_process, "resumed");
                    }
                } else {
                    start_new_process(current_process, getClk());
                    current_process->waiting_time = getClk() - current_process->P.ArrivalTime;
                    printSchedulerLogFile(current_process, "started");
                }
            }

            int rem2 = current_process->P.RemainingTime;
            if (rem2 <= 0) rem2 = current_process->P.Runtime;
            current_process->start_time = getClk();
            current_process->finish_time = current_process->start_time + ((rem2 >= quantum) ? quantum : rem2);
        }
    }
}

//Implement hpf 1-inher funcs
//              2-sched func

void Pri_inh(struct PCB* b, struct PCB* dep)
{
    if(b->P.Priority > dep->P.Priority)
    {
        dep->P.expri= dep->P.Priority; //store orig pri
        dep->P.Priority= b->P.Priority; //inherit from blocked
        dep->depp=true; //it has process depending on it
        dep->blockedID= b->P.PID; //blocked process
    }
}

void Pri_rev(struct PCB* dep){ //return to orig pri
    dep->P.Priority= dep->P.expri;
}

struct PCB* temp;
void HPF_Sched(int current_time){
    struct PCB* Hpri = peek(ready_queue); //get entry that is next

    if (current_process->P.PID == -1 && Hpri != NULL) { //if cpu free and there is a ready entry
        dequeue(&ready_queue, current_process); //deque ready to curr to run

        if (current_process->start_time == -1) { //never started before
            start_new_process(current_process, current_time);
            if(current_process->P.DependencyID != -1){ //depends
                temp = getPCB(current_process->P.DependencyID); //get pcb
                Pri_inh(current_process,temp); //inheritance, restored in main
                switch_process(current_process,temp,current_time,ALG_HPF); //switch so dependant is running
                current_process->state=BLOCKED; //no longer ready, get's freed in main
                current_process=temp;
            }
        } else {
            //check status (either run it or run dep)
            kill(current_process->P.PID, SIGCONT);
            current_process->state = RUNNING;
            if(current_process->P.DependencyID != -1){ //depends
                temp = getPCB(current_process->P.DependencyID);
                Pri_inh(current_process,temp); //inheritance, restored in main
                switch_process(current_process,temp,current_time,ALG_HPF); //switch so dependant is running
                current_process->state=BLOCKED; //get's freed in main
                current_process=temp;
            }
        }
    }
    else if (current_process->P.PID != -1 && Hpri != NULL){ //if cpu running a process
        if(current_process->P.Priority < Hpri->P.Priority){ //if entry is higher pri deq and make it curr (switch) and enq curr back into queue
            dequeue(&ready_queue, Hpri); 
        
            switch_process(current_process,Hpri, current_time,ALG_SRTN);
            current_process = Hpri;

            if(current_process->P.DependencyID != -1){
                temp = getPCB(current_process->P.DependencyID);
                Pri_inh(current_process,temp);
                switch_process(current_process,temp,current_time,ALG_HPF);
                current_process->state=BLOCKED;
                current_process=temp;
            }            
        }    
    }
}

int main(int argc, char * argv[])
{
    initClk(); 
    
    // ftok to match process gen
    key_t key_id = ftok("/tmp", 'M');
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
        struct PCB* depfree;
        int fid;
        while ((finished_pid = waitpid(-1, &status, WNOHANG)) > 0) { //wait for child to finish,if no one finished dont block
            update_alllist_finish(finished_pid, clock_time);
            
            // if finished process was running on CPU
            if (current_process->P.PID != -1 && current_process->P.PID == finished_pid) {
                current_process->P.RemainingTime = 0;
                current_process->state = FINISHED;
                current_process->P.PID = -1; //CPU is now Idle
                fid = current_process->blockedID; //free blocked process and restore pri
                if (fid != -1)
                {
                    depfree=getPCB(fid);
                    depfree->state= READY;
                    enqueue(&ready_queue, depfree, ALG_HPF);
                    Pri_rev(current_process);
                }
            }
        }

        //update remaining time
        // if (current_process->P.PID != -1) {
        //     current_process->P.RemainingTime--;
        // }

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
                enqueue(&ready_queue, &new_proc, ALG_SRTN);
            }
            else if (strcmp(alg_msg.mtext, "HPF") == 0) {
                // Enqueue logic for HPF 
                enqueue(&ready_queue, &new_proc, ALG_HPF);
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
            HPF_Sched(clock_time);
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

    
