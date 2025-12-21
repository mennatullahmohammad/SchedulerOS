#include "headers.h"
#include <stdlib.h>

int remainingtime;

int main(int argc, char * argv[])
{
    if (argc < 2) {
        fprintf(stderr, "process: missing runtime arg\n");
        return 1;
    }

    initClk();

    
    remainingtime = atoi(argv[1]);


    while (remainingtime > 0)
    {
        int cur = getClk();
        while (getClk() == cur) /* wait until clock increments */;
        remainingtime--;
    }
    key_t queue_key = ftok("/tmp", 'M');
    if (queue_key != -1) {
        int msgid = msgget(queue_key, 0666);
        if (msgid != -1) {
            struct FinishMsg finish_msg;
            finish_msg.mtype = 4;  // Type 4 for finish notifications
            finish_msg.pid = getpid();
            finish_msg.finish_time = getClk();
            
            msgsnd(msgid, &finish_msg, sizeof(finish_msg) - sizeof(long), 0);
        }
    }
    /* cleanup & exit to notify scheduler (SIGCHLD) */
    destroyClk(false);
    exit(0);
}