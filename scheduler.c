#include "headers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include "MMU.h"

#define MAX_PROCESSES 100
#define MSG_KEY 65

struct Node* ready_queue = NULL;   // For SRTN and HPF
struct Node* RR_head = NULL;       // For RR circular queue head
struct Node* RR_tail = NULL;       // For RR circular queue tail
struct PCB blocked_hpf[MAX_PROCESSES];
int bhpf = 0;

struct PCB* current_process = NULL;
struct PCB all_processes[MAX_PROCESSES];
int process_count = 0;

FILE* logFile = NULL;
int msgid = -1;

FILE* mem_log = NULL;
int sc_index = 0;   // Second Chance pointer

int quantum = 2;
int runningProcessStartTime = 0;
int runningProcessEndTime = 0;


void printSchedulerLogFile(struct PCB* p, const char* state);
struct PCB* find_master_by_pid(pid_t pid);
struct PCB* getPCB(int id);
void update_master_list_pid(int virt_id, pid_t os_pid, int start_time);
void update_master_list_finish(pid_t pid, int time);
void start_new_process(struct PCB* p, int current_time);
void switch_process(struct PCB* old_proc, struct PCB* new_proc, int current_time, Algorithm alg);
void SRTN_scheduler(int current_time);
void HPF_scheduler(int current_time);
void RR_scheduler(int current_time);
void Pri_inh(struct PCB* b, struct PCB* dep);
void Pri_rev(struct PCB* dep);
void generate_perf_file();
void cleanup_and_exit();
int second_chance_page();
int handle_page_fault(struct PCB* p, int vpn, int modify, int mode);
int MMU_access(struct PCB* p);

/* Memory Management (Phase 2) */
struct PCB blocked_list[MAX_PROCESSES];
int blocked_count = 0;

int MMU_access(struct PCB* p) {
    if (!p->req_file) return 0;

    int time_pstart = getClk() - p->start_time;
    
    // Keep reading until we find a request for current time or pass it
    while (1) {
        long file_line = ftell(p->req_file);
        char line[64];
        
        if (fgets(line, sizeof(line), p->req_file) == NULL) {
            return 0;  // EOF
        }

        if (line[0] == '#') continue;  // Skip comments, keep reading

        int req_time;
        char addrbin[32];
        char rw;
        sscanf(line, "%d %s %c", &req_time, addrbin, &rw);

        if (req_time > time_pstart) {
            // Future request, rewind and wait
            fseek(p->req_file, file_line, SEEK_SET);
            return 0;
        }
        
        if (req_time < time_pstart) {
            // Old request we missed, skip it
            continue;
        }

        // req_time == time_pstart, process this request
        int virtual_address = strtol(addrbin, NULL, 2);
        int vpn = virtual_address / PAGE_SIZE;
        int modified = (rw == 'w');

        if (p->page_table[vpn].valid == 1) {
            int frame = p->page_table[vpn].frame;
            frame_table[frame].ref = 1;
            p->page_table[vpn].ref = 1;

            if (modified) {
                frame_table[frame].modified = 1;
                p->page_table[vpn].modified = 1;
            }
            return 0;  // Hit, no blocking
        }

        // Page fault
        return handle_page_fault(p, vpn, modified, 0);
    }
}


// Get PCB by ID 
struct PCB* getPCB(int id) {
    for (int i = 0; i < process_count; i++) {
        if (all_processes[i].orig_ID == id) {
            return &all_processes[i];
        }
    }
    return NULL;
}

//Find master PCB by OS pid 
struct PCB* find_master_by_pid(pid_t pid) {
    for (int i = 0; i < process_count; i++) {
        if (all_processes[i].P.PID == pid) return &all_processes[i];
    }
    return NULL;
}

//Update master list (replace generator id with OS pid) 
void update_master_list_pid(int virt_id, pid_t os_pid, int start_time) {
    for (int i = 0; i < process_count; i++) {
        if (all_processes[i].orig_ID == virt_id) {
            all_processes[i].P.PID = os_pid;
            all_processes[i].start_time = start_time;
            return;
        }
    }
}

/* Update master list on finish */
void update_master_list_finish(pid_t pid, int time) {
    for (int i = 0; i < process_count; i++) {
        if (all_processes[i].P.PID == pid) {
            all_processes[i].finish_time = time;
            all_processes[i].state = FINISHED;
            all_processes[i].RemainingTime = 0;
            return;
        }
    }
}

// Logging helper 
void printSchedulerLogFile(struct PCB* p, const char* state) {
    if (!logFile || !p) return;
    int t = getClk();
    int printed_pid = p->orig_ID;

    if (strcmp(state, "started") == 0) {
        fprintf(logFile, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                t, printed_pid, p->P.ArrivalTime, p->P.Runtime, p->RemainingTime, p->waiting_time);
    } else if (strcmp(state, "resumed") == 0) {
        fprintf(logFile, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                t, printed_pid, p->P.ArrivalTime, p->P.Runtime, p->RemainingTime, p->waiting_time);
    } else if (strcmp(state, "stopped") == 0) {
        fprintf(logFile, "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                t, printed_pid, p->P.ArrivalTime, p->P.Runtime, p->RemainingTime, p->waiting_time);
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

// Start a new process 
void start_new_process(struct PCB* p, int current_time) {
    char str_runtime[32];
    snprintf(str_runtime, sizeof(str_runtime), "%d", p->P.Runtime);

    int virt_id = p->P.PID;

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        execl("./process.out", "process.out", str_runtime, NULL);
        perror("execl process.out");
        exit(1);
    }

    p->P.PID = pid;
    p->start_time = current_time;
    p->last_start_time = current_time;
    p->state = RUNNING;

    update_master_list_pid(virt_id, pid, current_time);
}

/* Switch between processes (for SRTN and HPF) */
void switch_process(struct PCB* old_proc, struct PCB* new_proc, int current_time, Algorithm alg) {
    kill(old_proc->P.PID, SIGSTOP);
    old_proc->last_start_time = current_time; //last started time
    old_proc->state = READY;
    enqueue(&ready_queue, old_proc, alg);

    if (new_proc->start_time == -1) {
        start_new_process(new_proc, current_time);
        new_proc->waiting_time = current_time - new_proc->P.ArrivalTime;
        printSchedulerLogFile(new_proc, "started");
    } else {
        kill(new_proc->P.PID, SIGCONT);
        new_proc->state = RUNNING;
        int time_in_queue = current_time - new_proc->last_start_time;
        new_proc->waiting_time += time_in_queue;
        new_proc->last_start_time = current_time;
        printSchedulerLogFile(new_proc, "resumed");
    }
}


//SRTN Scheduler 
void SRTN_scheduler(int current_time) {
    struct PCB* shortest_job = peek(ready_queue);

    // CPU is Idle, dequeue shortest job
    if (current_process->P.PID == -1 && shortest_job != NULL) {
        dequeue(&ready_queue, current_process);
        
        if (current_process->start_time == -1) {
            start_new_process(current_process, current_time);
            current_process->waiting_time = current_time - current_process->P.ArrivalTime;
            printSchedulerLogFile(current_process, "started");
        } else {
            kill(current_process->P.PID, SIGCONT);
            current_process->state = RUNNING;
            int time_in_queue = current_time - current_process->last_start_time;
            current_process->waiting_time += time_in_queue;
            current_process->last_start_time = current_time;
            printSchedulerLogFile(current_process, "resumed");
        }
    }
    // CPU is Busy, check for preemption
    else if (current_process->P.PID != -1 && shortest_job != NULL) {
        if (shortest_job->RemainingTime < current_process->RemainingTime) {
            struct PCB next_proc;
            dequeue(&ready_queue, &next_proc);
            
            printSchedulerLogFile(current_process, "stopped");
            switch_process(current_process, &next_proc, current_time, ALG_SRTN);
            
            *current_process = next_proc;
        }
    }
}

/* HPF Scheduler */
/* Priority Inheritance (for HPF) */
void Pri_inh(struct PCB* b, struct PCB* dep) {
    if (b->P.Priority > dep->P.Priority) {
        b->expri = dep->P.Priority; //save in blocked
        dep->P.Priority = b->P.Priority;
        dep->depp = true;
        dep->blockedID[dep->count] = b->orig_ID; //starting from 0
        dep->count++;
    }
}

void Pri_rev(struct PCB* dep) {
    //get first blocked id
    struct PCB* b = getPCB(dep->blockedID[0]);
    //put it's expri back
    dep->P.Priority = b->expri;
}

void depfound(struct PCB* curr, int ct)
{
    struct PCB* temp = getPCB(curr->P.DependencyID);
    if (temp == NULL){ //to avoid crashing
        fflush(stdout);
        return;
    }
    if (temp->state == FINISHED) {return;} //no need for inh
    Pri_inh(curr, temp);
    printSchedulerLogFile(curr, "stopped");
    kill(curr->P.PID, SIGSTOP);
    curr->last_start_time = ct; //last started time
    curr->state = BLOCKED;

    for (int i = 0; i < process_count; i++) { //set in all processes
        if (all_processes[i].orig_ID == curr->orig_ID) { //blocked proc
            all_processes[i].state = BLOCKED;
            all_processes[i].last_start_time = ct;
        }
        if (all_processes[i].orig_ID == temp->orig_ID) { //dep proc
            all_processes[i].P.Priority = temp->P.Priority;
            all_processes[i].depp = temp->depp;
            all_processes[i].count = temp->count;
            for (int j = 0; j < temp->count; j++) {
                all_processes[i].blockedID[j] = temp->blockedID[j];
            }
        }
    }

    blocked_hpf[bhpf++] = *curr; //put in array

    current_process->P.PID = -1; //idle
    current_process->state = READY;
}

void HPF_scheduler(int current_time) {
    struct PCB* Hpri = peek(ready_queue);

    if (current_process->P.PID == -1 && Hpri != NULL) {
        dequeue(&ready_queue, current_process);

        if (current_process->start_time == -1) { //never started before
            start_new_process(current_process, current_time);
            current_process->waiting_time = current_time - current_process->P.ArrivalTime;
            printSchedulerLogFile(current_process, "started");

            if (current_process->P.DependencyID != -1) { 
                struct PCB* dep = getPCB(current_process->P.DependencyID);
                if (dep && dep->state != FINISHED) {
                    depfound(current_process, current_time);
                    return;
                }
            }
        } else {
            kill(current_process->P.PID, SIGCONT);
            current_process->state = RUNNING;
            int time_in_queue = current_time - current_process->last_start_time;
            current_process->waiting_time += time_in_queue;
            current_process->last_start_time = current_time;
            printSchedulerLogFile(current_process, "resumed");
            
            if (current_process->P.DependencyID != -1) {
                struct PCB* dep = getPCB(current_process->P.DependencyID);
                if (dep && dep->state != FINISHED) {
                    depfound(current_process, current_time);
                    return;
                }
            }
        }
    }
    else if (current_process->P.PID != -1 && Hpri != NULL) { 
        if (current_process->P.Priority < Hpri->P.Priority) {
            struct PCB next_proc;
            dequeue(&ready_queue, &next_proc);
            
            printSchedulerLogFile(current_process, "stopped");
            switch_process(current_process, &next_proc, current_time, ALG_HPF);
            
            *current_process = next_proc;
            //&current_process = next_proc;

            if (current_process->P.DependencyID != -1) {
                struct PCB* dep = getPCB(current_process->P.DependencyID);
                if (dep && dep->state != FINISHED) {
                    depfound(current_process, current_time);
                    return;
                }
            }
        }
    }
}

// Round Robin Scheduler 
void RR_scheduler(int current_time) {
    // Check if current process finished execution completely
    if (current_process->P.PID != -1 && 
        current_process->state == RUNNING && 
        current_process->RemainingTime <= 0) {
        // Process finished ,will be reaped by waitpid in main loop
        current_process->P.PID = -1;
        current_process->state = FINISHED;
        runningProcessStartTime = 0;
        runningProcessEndTime = 0;
        return;
    }
    
    // CPU is idle - schedule next process from queue
    if (current_process->P.PID == -1) {
        if (RR_head != NULL) {
            struct PCB p = dequeueRR(&RR_head, &RR_tail);
            struct PCB* master = find_master_by_pid(p.P.PID);
        if (master && master->start_time != -1) {
            // Only sync fields that might have changed during blocking
            p.page_table = master->page_table;
            p.page_table_frame = master->page_table_frame;
            p.req_file = master->req_file;
        }
        
        *current_process = p;

            if (current_process->start_time == -1) {
                // First time starting
                start_new_process(current_process, current_time);
                current_process->waiting_time = current_time - current_process->P.ArrivalTime;
                printSchedulerLogFile(current_process, "started");
            } else {
                // Resuming from queue
                if (current_process->P.PID != -1 && kill(current_process->P.PID, SIGCONT) != -1) {
                    current_process->state = RUNNING;
                    // Waiting time already accumulated in main loop
                    current_process->last_start_time = current_time;
                    printSchedulerLogFile(current_process, "resumed");
                } else {
                    // Process was killed somehow, restart
                    start_new_process(current_process, current_time);
                    current_process->waiting_time = current_time - current_process->P.ArrivalTime;
                    printSchedulerLogFile(current_process, "started");
                }
            }

            int rem = current_process->RemainingTime;
            if (rem <= 0) rem = current_process->P.Runtime;
            runningProcessStartTime = current_time;
            runningProcessEndTime = current_time + ((rem >= quantum) ? quantum : rem);
        }
        return;
    }

    // Check if quantum expired for running process
    if (current_process->P.PID != -1 && 
        current_process->state == RUNNING && 
        current_time >= runningProcessEndTime) {
        
        pid_t pid = current_process->P.PID;

        // Check if process has more work to do
        if (current_process->RemainingTime > 0) {
            // Quantum expired, but process not finished - preempt
            printSchedulerLogFile(current_process, "stopped");

            if (kill(pid, SIGSTOP) == -1) {
                perror("kill SIGSTOP");
            }

            struct PCB copyPCB = *current_process;
            copyPCB.state = READY;
            copyPCB.last_start_time = current_time;
            enqueueRR(&RR_head, &RR_tail, copyPCB);

            current_process->P.PID = -1;
            current_process->state = READY;
            runningProcessStartTime = 0;
            runningProcessEndTime = 0;
        }
        // If RemainingTime <= 0, process will finish naturally
    }
}
/* Generate performance statistics file */
void generate_perf_file() {
    double total_wta = 0.0;
    double total_waiting = 0.0;
    int finished_count = 0;
    int total_runtime = 0;
    int last_finish_time = 0;

    for (int i = 0; i < process_count; i++) {
        if (all_processes[i].state == FINISHED) {
            int TA = all_processes[i].finish_time - all_processes[i].P.ArrivalTime;
            double WTA = (double)TA / (double)all_processes[i].P.Runtime;
            int wait = TA - all_processes[i].P.Runtime;
            
            total_wta += WTA;
            total_waiting += wait;
            total_runtime += all_processes[i].P.Runtime;
            finished_count++;
            
            if (all_processes[i].finish_time > last_finish_time)
                last_finish_time = all_processes[i].finish_time;
        }
    }

    double avg_wta = (finished_count > 0) ? total_wta / finished_count : 0.0;
    double avg_waiting = (finished_count > 0) ? total_waiting / finished_count : 0.0;

    double variance = 0.0;
    for (int i = 0; i < process_count; i++) {
        if (all_processes[i].state == FINISHED) {
            int TA = all_processes[i].finish_time - all_processes[i].P.ArrivalTime;
            double WTA = (double)TA / (double)all_processes[i].P.Runtime;
            variance += (WTA - avg_wta) * (WTA - avg_wta);
        }
    }
    double std_wta = (finished_count > 0) ? sqrt(variance / finished_count) : 0.0;

    double cpu_util = (last_finish_time > 0) ? 
        (100.0 * total_runtime) / last_finish_time : 0.0;

    FILE* perfFile = fopen("scheduler.perf", "w");
    if (perfFile) {
        fprintf(perfFile, "CPU utilization = %.2f%%\n", cpu_util);
        fprintf(perfFile, "Avg WTA = %.2f\n", avg_wta);
        fprintf(perfFile, "Avg Waiting = %.2f\n", avg_waiting);
        fprintf(perfFile, "Std WTA = %.2f\n", std_wta);
        fclose(perfFile);
        printf("Performance file generated successfully\n");
        fflush(stdout);
    } else {
        perror("fopen scheduler.perf");
    }
}

void cleanup_and_exit() {
    generate_perf_file();
    for (int i = 0; i < process_count; i++) {
        if (all_processes[i].req_file) {
            fclose(all_processes[i].req_file);
        }
    }
    
    if (msgid != -1) msgctl(msgid, IPC_RMID, NULL);
    if (mem_log) fclose(mem_log);  
    if (logFile) fclose(logFile);
    destroyClk(true);
    exit(0);
}

int second_chance_page(){
    while(frame_table[sc_index].ref == 1)
    {
        frame_table[sc_index].ref = 0;
        sc_index++;

        if (sc_index == FRAME_COUNT)
            {sc_index =0;}
    }
        
    int page = sc_index;
    sc_index++;

    if (sc_index == FRAME_COUNT)
        {sc_index =0;}

    return page;
}

// Mode parameter:
// any number except 1 = Regular page fault
// 1 = Initial page table allocation
int handle_page_fault(struct PCB* p, int vpn, int modify, int mode)
{
    int frame;

    //MODE 1: INITIAL PAGE TABLE ALLOCATION 
    if (mode == 1)
    {
        frame = find_free_frame();
        if (frame == -1)
            frame = second_chance_page();

            frame_table[frame].free = 0;
            frame_table[frame].pid = p->P.PID;
            frame_table[frame].vpn = -1;     // page table vpn = -1
            frame_table[frame].ref = 1;
            frame_table[frame].modified = 0;
            p->page_table_frame = frame;

        fprintf(mem_log, "At time %d page table of process %d allocated at frame %d\n", getClk(), p->P.PID, frame);

        fflush(mem_log);
        return 0;   // doesn't block scheduler
    }

    fprintf(mem_log, "PageFault upon VA %d from process %d\n", vpn, p->P.PID);
    frame = find_free_frame();

    if (frame == -1)
    {
        frame = second_chance_page();
    

        struct Frame* selected_page = &frame_table[frame];

        while (selected_page->vpn == -1)
        {
            selected_page->ref = 0;
            frame = second_chance_page();
            selected_page = &frame_table[frame];
        }

        struct PCB* selected_page_proc = find_master_by_pid(selected_page->pid);

        if (selected_page->modified) {
            fprintf(mem_log, "Swapping out page %d to disk\n", selected_page->vpn);
        }

        selected_page_proc->page_table[selected_page->vpn].valid = 0;
    }
    else {
        fprintf(mem_log, "Free Physical page %d allocated\n", frame);
    }

    frame_table[frame].free = 0;
    frame_table[frame].pid = p->P.PID;
    frame_table[frame].modified = modify;
    frame_table[frame].ref = 1;
    frame_table[frame].vpn = vpn;

    p->page_table[vpn].modified = modify;
    p->page_table[vpn].frame = frame;
    p->page_table[vpn].valid = 1;
    p->page_table[vpn].ref = 1;

    fprintf(mem_log, "At time %d page %d for process %d is loaded into memory page %d.\n", getClk(), vpn, p->P.PID, frame);

    fflush(mem_log);

    return 1;   

}



int main(int argc, char* argv[]) {
    init_memory(); 
    int generator_done = 0;
    initClk();
    mem_log = fopen("memory.log", "w");
    if (!mem_log) {
        perror("fopen memory.log");
        destroyClk(true);
        return 1;
    }
    fprintf(mem_log, "#At time x page y for process z is loaded into memory page w.\n");
    fprintf(mem_log, "#PageFault upon VA 'xx' from process 'yy' \n");
    fprintf(mem_log, "#Free physical page 'zz' allocated\n");
    fprintf(mem_log, "#Swapping out page 'zz' to disk\n");
    fflush(mem_log);

    key_t queue_key = ftok("/tmp", 'M');
    if (queue_key == -1) {
        perror("ftok");
        destroyClk(true);
        return 1;
    }
    msgid = msgget(queue_key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        destroyClk(true);
        return 1;
    }

    struct AlgorithmMsg alg_msg;
    if (msgrcv(msgid, &alg_msg, sizeof(alg_msg.mtext), 1, 0) == -1) {
        perror("msgrcv alg");
        cleanup_and_exit();
    }

    printf("Received algorithm: %s\n", alg_msg.mtext);
    fflush(stdout);

    if (strncmp(alg_msg.mtext, "RR", 2) == 0) {
        char* sp = strchr(alg_msg.mtext, ' ');
        if (sp) {
            int q = atoi(sp + 1);
            if (q > 0) quantum = q;
        }
        printf("Running Round Robin with quantum = %d\n", quantum);
        fflush(stdout);
    }

    current_process = malloc(sizeof(struct PCB));
    if (!current_process) {
        perror("malloc");
        cleanup_and_exit();
    }
    current_process->P.PID = -1;
    current_process->start_time = -1;
    current_process->state = READY;
    current_process->waiting_time = 0;
    current_process->last_start_time = -1;
    current_process->blocked = 0;
    current_process->unblock_time = -1;
    //current_process->blockedID = -1;
    //initialize everything??
    current_process->depp = false;
    current_process->count = 0;

    logFile = fopen("scheduler.log", "w");
    if (!logFile) {
        perror("fopen scheduler.log");
        cleanup_and_exit();
    }
    fprintf(logFile, "#At time x process y state arr w total z remain y wait k\n");
    fflush(logFile);
    printf("Log file opened successfully\n");
    fflush(stdout);

    int prev_time = -1;

    while (1) {
        int clock_time = getClk();
        if (clock_time == prev_time) {
            usleep(1000);
            continue;
        }
        prev_time = clock_time;

        /* Wake up BLOCKED processes */
        for (int i = 0; i < blocked_count; i++) {
    if (clock_time >= blocked_list[i].unblock_time) {
        // Get FRESH copy from master
        struct PCB* master = find_master_by_pid(blocked_list[i].P.PID);
        if (master) {
            master->blocked = 0;
            master->state = READY;
            master->last_start_time = clock_time;

            if (strncmp(alg_msg.mtext, "RR", 2) == 0) {
                enqueueRR(&RR_head, &RR_tail, *master);  // Use fresh copy
            } else {
                Algorithm alg = (strcmp(alg_msg.mtext, "SRTN") == 0) ? ALG_SRTN : ALG_HPF;
                enqueue(&ready_queue, master, alg);
            }
        }

        // Remove from blocked list
        for (int j = i; j < blocked_count - 1; j++)
            blocked_list[j] = blocked_list[j + 1];
        blocked_count--;
        i--;
    }
}
        //notification from process that it finished, dont blocl
        struct FinishMsg finish_msg;
        while (msgrcv(msgid, &finish_msg, sizeof(finish_msg), 4, IPC_NOWAIT) != -1) {
            pid_t finished_pid = finish_msg.pid;
            
            // Reap the finished process
            int status;
            waitpid(finished_pid, &status, 0);
            
            update_master_list_finish(finished_pid, clock_time);

            struct PCB* m = find_master_by_pid(finished_pid);
            if (m != NULL) {
                m->finish_time = clock_time;
                m->state = FINISHED;
                m->waiting_time = (clock_time - m->P.ArrivalTime) - m->P.Runtime;

                 if (m->page_table) {
                    free(m->page_table);
                    m->page_table = NULL;
                }

                printSchedulerLogFile(m, "finished");

                // HPF: Free blocked processes with priority inheritance
                if (strcmp(alg_msg.mtext, "HPF") == 0 && m->count != 0) {
                    for (int i=0 ; i<m->count ; i++)
                    {
                        int blckd = m->blockedID[i]; //get blocked proc

                        for(int j=0; j<bhpf ; j++){
                            if (blocked_hpf[j].orig_ID == blckd) //look for in blocked array
                            {
                                for (int k = 0; k < process_count; k++) { //set and return
                                    if (all_processes[k].orig_ID == blckd) {
                                        all_processes[k].state = READY;
                                        all_processes[k].last_start_time = clock_time;
                                        enqueue(&ready_queue, &all_processes[k], ALG_HPF);
                                        break;
                                    }
                                }
                                for (int k = j; k < bhpf - 1; k++) { //shift to remove
                                    blocked_hpf[k] = blocked_hpf[k + 1];
                                }
                                bhpf--;
                                j--;  // Recheck this index
                                break;
                            }
                        }
                    }
                    Pri_rev(m);
                    m->depp = false;
                    m->count = 0;
                }
            }

            if (current_process->P.PID == finished_pid) {
                current_process->P.PID = -1;
                current_process->state = FINISHED;
            }
        }

        /* Update remaining time for running process */
        if (current_process->state == RUNNING && current_process->P.PID != -1) {
            if (current_process->RemainingTime > 0) {
                current_process->RemainingTime--;
            }
        }
        
        /* Memory access while RUNNING (Phase 2) */
        if (current_process->state == RUNNING && current_process->P.PID != -1) {
    int mmu_result = MMU_access(current_process);

    if (mmu_result == 1) {
    // First, update master with current state
    struct PCB* master = find_master_by_pid(current_process->P.PID);
    if (master) {
        // Copy current state to master
        master->RemainingTime = current_process->RemainingTime;
        master->waiting_time = current_process->waiting_time;
        master->blocked = 1;
        master->state = BLOCKED;
        master->unblock_time = clock_time + 10;
        master->last_start_time = clock_time;
        
        printSchedulerLogFile(master, "stopped");
        kill(master->P.PID, SIGSTOP);
        
        // Now copy to blocked_list
        blocked_list[blocked_count++] = *master;
    }

    current_process->P.PID = -1;
    current_process->state = READY;
    runningProcessStartTime = 0;
    runningProcessEndTime = 0;
}
}

        /* Receive new processes */
        struct ProcessMsg proc_msg;
        struct DoneMsg done_msg; 
        while (msgrcv(msgid, &proc_msg, sizeof(proc_msg.proc), 2, IPC_NOWAIT) != -1) {
            struct PCB new_proc;
            new_proc.P= proc_msg.proc;
            new_proc.orig_ID = proc_msg.proc.PID; //save original ID


            printf("Received process: PID=%d, Arrival=%d, Runtime=%d at time %d\n",
                   new_proc.P.PID, new_proc.P.ArrivalTime, new_proc.P.Runtime, clock_time);
            fflush(stdout);

            if (new_proc.P.Runtime <= 0)
                new_proc.P.Runtime = 1;

            new_proc.RemainingTime = new_proc.P.Runtime;
            new_proc.state = READY;
            new_proc.start_time = -1;
            new_proc.finish_time = -1;
            new_proc.waiting_time = 0;
            new_proc.last_start_time = -1;
            new_proc.blocked = 0;
            new_proc.unblock_time = -1;
            //new_proc.blockedID = -1; it's an array
            new_proc.depp = false;
            new_proc.count = 0; //for blocked array
            
            char req_filename[64];
            snprintf(req_filename, sizeof(req_filename), "requests_%d.txt", new_proc.P.PID);
            new_proc.req_file = fopen(req_filename, "r");
            if (!new_proc.req_file) {
                printf("Warning: Could not open %s\n", req_filename);
                
            }

            new_proc.num_pages = (new_proc.P.disk_limit + PAGE_SIZE - 1) / PAGE_SIZE;

            new_proc.page_table = malloc(new_proc.num_pages * sizeof(struct PageTableEntry));

            for (int i = 0; i < new_proc.num_pages; i++) {
            new_proc.page_table[i].valid = 0;
            new_proc.page_table[i].frame = -1;
            new_proc.page_table[i].ref = 0;
            new_proc.page_table[i].modified = 0;
            }

            handle_page_fault(&new_proc, -1, 0, 1); //initialize new page table vpn = -1
            handle_page_fault(&new_proc, 0, 0, 0);  //load first page vpn = 0

            if (process_count < MAX_PROCESSES)
                all_processes[process_count++] = new_proc;

            if (strcmp(alg_msg.mtext, "SRTN") == 0) {
                enqueue(&ready_queue, &new_proc, ALG_SRTN);
            } else if (strcmp(alg_msg.mtext, "HPF") == 0) {
                enqueue(&ready_queue, &new_proc, ALG_HPF);
            } else if (strncmp(alg_msg.mtext, "RR", 2) == 0) {
                enqueueRR(&RR_head, &RR_tail, new_proc);
            }
        }

        /* Run appropriate scheduler */
        if (strcmp(alg_msg.mtext, "SRTN") == 0) {
            SRTN_scheduler(clock_time);
        } 
        else if (strcmp(alg_msg.mtext, "HPF") == 0) {
            HPF_scheduler(clock_time);
        } 
        else if (strncmp(alg_msg.mtext, "RR", 2) == 0) {
            struct Node* temp = RR_head;
            while (temp != NULL) {
                if (temp->Entry.state == READY &&
                    current_process->P.PID != temp->Entry.P.PID) {
                    temp->Entry.waiting_time++;
                }
                temp = temp->next;
                if (temp == RR_head) break; 
            }
            RR_scheduler(clock_time);
        }
        
        

        /* Termination check */
        if (process_count > 0 ) {
            int all_done = 1;
            for (int i = 0; i < process_count; i++) {
                if (all_processes[i].state != FINISHED) {
                    all_done = 0;
                    break;
                }
            }

       if (!generator_done && msgrcv(msgid, &done_msg, sizeof(done_msg.dummy), 3, IPC_NOWAIT) != -1)
        {
            generator_done = 1;
            printf("Generator done received at time %d, generatordone = %d\n", getClk(), generator_done);
        }

            int queue_empty = (strncmp(alg_msg.mtext, "RR", 2) == 0) ? 
                              (RR_head == NULL) : (ready_queue == NULL);

            if (generator_done && all_done && queue_empty && current_process->P.PID == -1) {
                printf("All %d processes finished at time %d\n", process_count, clock_time);
                fflush(stdout);
                break;
            }
        }

        if (clock_time > 1000) {
            printf("Timeout at time %d with %d processes\n", clock_time, process_count);
            fflush(stdout);
            break;
        }
    }

    generate_perf_file();

    if (msgid != -1) msgctl(msgid, IPC_RMID, NULL);
    if (logFile) fclose(logFile);
    free(current_process);
    destroyClk(true);
    
    printf("Scheduler terminated successfully\n");
    return 0;
}