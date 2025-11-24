#include "headers.h"


int main(int argc, char * argv[])
{
    initClk(); 
    //TODO implement the scheduler :)
    //upon termination release the clock resources.



    
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
    
   
    destroyClk(true);

    return 0;
}



