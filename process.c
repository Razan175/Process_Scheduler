#include "headers.h"
#include <signal.h>

int remainingtime;
int id;

void handler_done(int sig_num)
{
    destroyClk(false);
    exit(id);
}

int main(int argc, char *argv[]) {
    // Signal handling
    signal(SIGINT,handler_done);

    remainingtime = atoi(argv[1]);
    id = atoi(argv[2]);
    
    printf("Process %d started with remaining time: %d\n", id, remainingtime);
    
    initClk();
    while (remainingtime > 0) {
        
            // Simulate work
            sleep(1);
            remainingtime--;            
    }
    
    raise(SIGINT);

}

