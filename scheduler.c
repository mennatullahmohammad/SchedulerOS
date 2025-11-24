#include "headers.h"

#define MAX_PROCESSES 100
#define MSG_KEY 65   // same as ftok in generator

// ====================== Ready Queue ======================
struct PCB* ready_queue[MAX_PROCESSES];
int heap_size = 0;

struct PCB* all_processes[MAX_PROCESSES];
int process_count = 0;

struct PCB* current = NULL;
int clock_time = 0;
int cpu_busy_time = 0;

FILE *log_file;
FILE *perf_file;
// integer square root
double sqrt(double x) {
    double guess = x / 2.0;
    if (x == 0) return 0;
    for(int i = 0; i < 20; i++) { // 20 iterations of Newton-Raphson
        guess = (guess + x / guess) / 2.0;
    }
    return guess;
}

// -------- Heap functions --------
void swap(int i, int j) {
    struct PCB* tmp = ready_queue[i];
    ready_queue[i] = ready_queue[j];
    ready_queue[j] = tmp;
}

void heapify_up(int index) {
    if (index == 0) return;
    int parent = (index - 1) / 2;
    if (ready_queue[index]->remaining_time < ready_queue[parent]->remaining_time ||
        (ready_queue[index]->remaining_time == ready_queue[parent]->remaining_time &&
         ready_queue[index]->arrival_time < ready_queue[parent]->arrival_time)) {
        swap(index, parent);
        heapify_up(parent);
    }
}

void heapify_down(int index) {
    int left = 2*index+1, right = 2*index+2, smallest=index;
    if (left < heap_size &&
        (ready_queue[left]->remaining_time < ready_queue[smallest]->remaining_time ||
        (ready_queue[left]->remaining_time == ready_queue[smallest]->remaining_time &&
         ready_queue[left]->arrival_time < ready_queue[smallest]->arrival_time)))
        smallest = left;
    if (right < heap_size &&
        (ready_queue[right]->remaining_time < ready_queue[smallest]->remaining_time ||
        (ready_queue[right]->remaining_time == ready_queue[smallest]->remaining_time &&
         ready_queue[right]->arrival_time < ready_queue[smallest]->arrival_time)))
        smallest = right;
    if (smallest != index) { swap(index, smallest); heapify_down(smallest); }
}

void push_ready_queue(struct PCB* p) {
    ready_queue[heap_size] = p;
    heap_size++;
    heapify_up(heap_size-1);
}

struct PCB* pop_ready_queue() {
    if (heap_size == 0) return NULL;
    struct PCB* top = ready_queue[0];
    ready_queue[0] = ready_queue[heap_size-1];
    heap_size--;
    heapify_down(0);
    return top;
}

struct PCB* peek_ready_queue() {
    if (heap_size == 0) return NULL;
    return ready_queue[0];
}

// ====================== Logging ======================
void log_event(const char* event, struct PCB* p, int time) {
    fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d\n",
            time, p->pid, event, p->arrival_time, p->burst_time, p->remaining_time);
    fflush(log_file);
}

// ====================== Process Control ======================
void start_process(struct PCB* p, int current_time) {
    pid_t child = fork();
    if (child == 0) {
        raise(SIGSTOP); // pause until scheduler resumes
        char arg_id[16], arg_burst[16];
        sprintf(arg_id, "%d", p->pid);
        sprintf(arg_burst, "%d", p->burst_time);
        execl("./process","process",arg_id,arg_burst,NULL);
        perror("execl"); exit(1);
    } else if (child > 0) {
        p->pid = child;
        p->state = RUNNING;
        if (p->start_time == -1) p->start_time = current_time;
        p->last_start_time = current_time;
        kill(child, SIGCONT);
        log_event("Started", p, current_time);
    } else { perror("fork"); exit(1); }
}

void preempt(struct PCB* old, int current_time) {
    kill(old->pid, SIGSTOP);
    old->state = READY;
    int ran = current_time - old->last_start_time;
    old->remaining_time -= ran;
    if (old->remaining_time < 0) old->remaining_time = 0;
    push_ready_queue(old);
    log_event("Stopped", old, current_time);
}

void resume(struct PCB* p, int current_time) {
    kill(p->pid, SIGCONT);
    p->state = RUNNING;
    if (p->start_time == -1) p->start_time = current_time;
    p->last_start_time = current_time;
    log_event("Resumed", p, current_time);
}

// ====================== SIGCHLD Handler ======================
void sigchld_handler(int sig) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < process_count; i++) {
            if (all_processes[i]->pid == pid) {
                struct PCB* p = all_processes[i];
                p->finish_time = clock_time;
                p->state = FINISHED;
                log_event("Finished", p, clock_time);
                current = NULL;
                break;
            }
        }
    }
}



int main(int argc, char * argv[])
{
    initClk();
    
    //TODO implement the scheduler :)
    //upon termination release the clock resources.



    //****mariam******* */
    //ready queue -> queues from ds
    //1-Implement Shortest Remaining Time Next (SRTN) (preemptive shortest remaining time).

    //2- Implement process start: fork + exec (give child its params).

    //3- Implement process switch: stop old process (SIGSTOP), save state (PCB fields), resume new (SIGCONT).
    //take input to see which process to run

    //4- Maintain PCB for each process (state, remaining, arrival, start/finish times, pid, wait times, WTA/TA).

    //5- Maintain ready queue optimized for SRTN.

    //6-React to process arrivals (from process_generator) and process termination notifications (from children / process).

    //7-Produce Scheduler.log (events) and Scheduler.perf (final metrics).

    //8-Clean up IPC on exit.
    
    signal(SIGCHLD, sigchld_handler);

    log_file = fopen("Scheduler.log", "w");
    perf_file = fopen("Scheduler.perf", "w");

    int msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
    if (msgid == -1) { perror("msgget"); exit(1); }

    // 1) Receive algorithm name first
    struct AlgorithmMsg alg_msg;
    if (msgrcv(msgid, &alg_msg, sizeof(alg_msg.mtext), 1, 0) == -1) {
        perror("msgrcv alg"); exit(1);
    }
    printf("Selected Algorithm: %s\n", alg_msg.mtext);

    while (1) {
        clock_time++;

        // 2) Receive new arrivals
        struct ProcessMsg proc_msg;
        while (msgrcv(msgid, &proc_msg, sizeof(proc_msg.process), 2, IPC_NOWAIT) != -1) {
            struct PCB* p = malloc(sizeof(struct PCB));
            p->pid = proc_msg.process.PID;
            p->arrival_time = proc_msg.process.ArrivalTime;
            p->burst_time = proc_msg.process.Runtime;
            p->remaining_time = proc_msg.process.Runtime;
            p->start_time = -1;
            p->finish_time = -1;
            p->last_start_time = -1;
            p->waiting_time = 0;
            p->state = READY;
            all_processes[process_count++] = p;
            push_ready_queue(p);
            log_event("Arrived", p, clock_time);
        }

        // 3) Decide who to run
        struct PCB* top = peek_ready_queue();
        if (current == NULL && top != NULL) {
            current = pop_ready_queue();
            start_process(current, clock_time);
        } else if (current != NULL && top != NULL && top->remaining_time < current->remaining_time) {
            preempt(current, clock_time);
            current = pop_ready_queue();
            resume(current, clock_time);
        }

        // 4) Update running process
        if (current != NULL) {
            current->remaining_time -= 1;
            cpu_busy_time++;
        }

        // 5) Check termination
        int done = 1;
        for (int i = 0; i < process_count; i++)
            if (all_processes[i]->state != FINISHED) done = 0;
        if (done && heap_size == 0 && current == NULL) break;

        sleep(1);
    }

    // ====================== Performance Metrics ======================
    double total_wta = 0, total_wt = 0, std_wta = 0;
    for (int i = 0; i < process_count; i++) {
        struct PCB* p = all_processes[i];
        int ta = p->finish_time - p->arrival_time;
        double wta = (double)ta / p->burst_time;
        total_wta += wta;
        total_wt += ta - p->burst_time;
    }
    double avg_wta = total_wta / process_count;
    double avg_wt = total_wt / process_count;
    for (int i = 0; i < process_count; i++) {
        struct PCB* p = all_processes[i];
        int ta = p->finish_time - p->arrival_time;
        double wta = (double)ta / p->burst_time;
        std_wta += (wta - avg_wta) * (wta - avg_wta);
    }
    std_wta = sqrt(std_wta / process_count);

    fprintf(perf_file,"CPU Utilization: %.2f\n", 100.0 * cpu_busy_time / clock_time);
    fprintf(perf_file,"Average WTA: %.2f\n", avg_wta);
    fprintf(perf_file,"Average WT: %.2f\n", avg_wt);
    fprintf(perf_file,"Std WTA: %.2f\n", std_wta);

    fclose(log_file);
    fclose(perf_file);
    msgctl(msgid, IPC_RMID, NULL);

    return 0;
        
    destroyClk(true);
}
