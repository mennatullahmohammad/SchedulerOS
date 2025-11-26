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

    /* cleanup & exit to notify scheduler (SIGCHLD) */
    destroyClk(false);
    exit(0);
}