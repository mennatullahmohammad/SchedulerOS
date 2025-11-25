#include "headers.h"

#define MAX_PROCESSES 100
#define MSG_KEY 65   // same as ftok in generator

void STRN(){
    
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
  


    /*
    * Implement the Shortest Remaining Time Next (SRTN) scheduling algorithm.
    * Implement the logic to switch between two processes (stop the old, save its state, start/resume another).
    * Implement the logic to start a new process (fork it and give it its parameters).
    * Manage the Process Control Block (PCB) for each process, keeping track of its state (running/waiting), remaining time, etc..
    * Define and manage the ready queue data structure for the SRTN algorithm.
    */

    return 0;
        
    destroyClk(true);
}
