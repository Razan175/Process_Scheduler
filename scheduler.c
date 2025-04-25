#include "headers.h"


int main(int argc, char *argv[])
{
    initClk();

    //TODO: implement the scheduler.
    //TODO: upon termination release the clock resources.
    int algo=argv[1];
    int process_count=atoi(argv[2]);
    int quantum;
    if(argc>3)
    {
       quantum=argv[3]; 
    }
    int attach=shmat(IPC_PRIVATE,0,SHM_RDONLY);
    int schulder=fork();
    switch (algo)
   {
   case 1:
   excel("./RR.out","./RR.out",NULL);
    break;
   case 2:
   excel("./STRN.out","./STRN.out",NULL);
   case 3:
   excel("./HPF.out","./HPF.out",NULL);
   default:
    break;
   }
    

    destroyClk(true);
}
//argv contains: algorithm number, process_count_str(idk what that is), quantum if exists
    //attach to shared memory with the process generator
    /*we will need a PCB struct, it will contain:
        process state (running/waiting)
        execution time
        remaining time
        waiting time
        might need to add more
    */
   
    //we'll have a switch case over the algorithm number
    /*
        for all algorithms:
        -every second check if there is a new process arrived and add it to the queue
            non preemptive: check if the running process is done
            preemptive: check if the running process needs to be replaced
        -do needed check for which process gets a turn
        -when starting a new process, fork and execl a process
            give the remaining time as a parameter 
        -the process should send a signal that it terminated,the handler deletes its data and send a signal to the process_generator
        -do needed calculations for:
            CPU utilization
            average weighted turnaround
            average waiting time
            standard deviation for average weighted turnaround time
        -generate output file
    */

