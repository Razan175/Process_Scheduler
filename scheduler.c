#include "headers.h"
#include "Defined_DS.h"

void HPF(int process_count);
void HPFhandler(int sig_num);

void RR(int process_count, int quantum);

void SRTN(int process_count);

int main(int argc, char *argv[]) //1- alognumber,2- process_count,3- quantum
{
    initClk();

    signal(SIGCHLD,HPFhandler);    
    
    int algo = atoi(argv[1]);

    printf("Scheduler PID: %d \n",getpid());

    switch (algo)
    {
        case 1:
            RR(atoi(argv[2]), atoi(argv[3]));
        case 2:
            SRTN(atoi(argv[2]));
        case 3:
            HPF(atoi(argv[2]));
        default:
            return EXIT_FAILURE;
    }
    destroyClk(false);
}

////////////////////////////////////////// Highest Priority first ////////////////////////////////////////

PriorityQueue *Process_queue;
int completed;
struct Process runningProcess = {0,0,0,0,0,0,finished};
struct PCB* pcb;
int pcbidx = 0;     //tracks the current running process index

void HPF(int process_count)
{
    key_t msgkey = ftok("keyfile",'p');
    int attach = msgget(msgkey, 0666 | IPC_CREAT);
    if(attach == -1)
    {
        perror("message queue not attached");
    }
    Process_queue = createPriorityQueue(process_count);
        
    //tracks the number of processes that own a PCB    
    int pcbSize = 0;

    pcb = (struct PCB*)malloc(sizeof(PCB) * process_count);
    int pid;

    initClk();
    while(completed < process_count)
    {
        if (Process_queue->size > 0)
        {
            //if the current running process is finished or if this is the first process in the system
            if (pcb[pcbidx].processstate == finished || pcbSize == 1)   {
                runningProcess = removePriorityPriorityQueue(Process_queue);
                runningProcess.processstate = running; 
                pid = fork();

                if (pid == 0)
                {
                    char rt[10], id[10];                                 
                    sprintf(rt, "%d", runningProcess.runtime);
                    sprintf(id, "%d", runningProcess.id);
                    execl("./process.out","./process.out",rt,id,NULL);
                }
                else
                {   
                    //sets the needed PCB info of the current running process
                    pcbidx = runningProcess.id - 1;     
                    pcb[pcbidx].waitingtime = getClk() - newMessage.p.arrival_time;
                    pcb[pcbidx].remainingtime = newMessage.p.runtime;
                    pcb[pcbidx].executiontime = getClk();
                    pcb[pcbidx].processstate = running;
                }                  
            }
        }
    
           
        if (msgrcv(attach,&newMessage,sizeof(newMessage.p),0,!IPC_NOWAIT) != -1)
        {
            insertPriorityPriorityQueue(Process_queue, newMessage.p);
            pcb[pcbSize].processstate = ready; //The test generator sets process ids from 1->process_count in order                                         
            pcb[pcbSize++].p = newMessage.p;    //We will access PCB of a certain process through its id
        }
        else
        {
            perror("Error receiveing process to scheduler");
        }
    }

    free(pcb);
    destroyPriorityQueue(Process_queue);
    msgctl(attach,IPC_RMID,NULL);
    destroyClk(false);
}

void HPFhandler(int sig_num)
{
    //sets the needed pcb info of the finished process
    pcb[pcbidx].finishedtime = getClk();
    pcb[pcbidx].processstate = finished;
    runningProcess.processstate = finished;
    completed++;
    kill(getppid(),SIGUSR1);
}

//////////////////////////////////// Round Robin ///////////////////////////////////////////////////////
void RR(int process_count, int quantum)
{

}


//////////////////////////////////// Shortest Remaining Time Next //////////////////////////////////////
void SRTN(int process_count)
{
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
   //struct msgbuff newMessage;
