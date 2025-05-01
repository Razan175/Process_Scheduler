#include "headers.h"
#include <signal.h>

int remainingtime;
int quantum;
int current_pid;
int paused = 0;
int oldtime=0;
void handle_stop(int sig) 
{
    paused = 1;
    
    printf("Process %d paused.\n", current_pid);
    //while (paused) {
        //sleep(1);  // Sleep while paused to simulate a halt
   // }
   
}

void handle_stop2(int sig) 
{
    paused = 1;
    remainingtime++;
    printf("Process %d paused.\n", current_pid);
    while (paused) {
        //sleep(1);  // Sleep while paused to simulate a halt
    }
   
}

void handle_cont(int sig) 
{
    paused = 0;
    printf("Process %d resumed.\n", current_pid);
}

int id;

void handler_done(int sig_num)
{
    destroyClk(false);
    exit(id);
}
int main(int argc, char *argv[]) {
    // Signal handling

    signal(SIGSTOP, handle_stop);
    signal(SIGTSTP,handle_stop2);
    signal(SIGCONT, handle_cont);
    signal(SIGINT,handler_done);

    current_pid = getpid();
   

    remainingtime = atoi(argv[1]);
    id = atoi(argv[2]);
    //int algo = atoi(argv[2]);
    //quantum = (argc > 3) ? atoi(argv[3]) : 1;
    
    printf("Process %d started with remaining time: %d\n", id, remainingtime);
    
    initClk();
    int t = getClk() + 1;
    while (remainingtime > 0) {
        //if (!paused)
         {
            // Simulate work
            sleep(1);
            //while (t > getClk());
            //t++;
            //oldtime = remainingtime;
            //int current_time = getClk();
            
            remainingtime--;
            printf("Process %d remaining time: %d current time: %d\n", current_pid, remainingtime, getClk());
            
            //while (getClk() < current_time + 1)
            //{
                // absolute cinema
            //}
            
        }
    }
    raise(SIGINT);
    destroyClk(false);
    return 0;
}

