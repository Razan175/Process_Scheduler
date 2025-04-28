#include "headers.h"
#include "Defined_DS.h"

void HPF(int process_count);
void HPFhandler(int sig_num);

void RR(int process_count, int quantum);
void RRhandler(int sig_num);
void SRTN(int process_count);

int main(int argc, char *argv[]) //1- alognumber,2- process_count,3- quantum
{
    initClk();

    signal(SIGCHLD,HPFhandler);  
    FILE *logFile = fopen("scheduler.log", "w");
    FILE *perfFile = fopen("scheduler.perf", "w");

if (logFile == NULL || perfFile == NULL) {
    perror("Error opening output files");
    exit(1);
}

    
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
fclose(logFile);
fclose(perfFile);

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
            //if the current running process is finished or if this is the first process 
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
//--------------------------------------------------------------------
//////////////////////////////////// Round Robin ///////////////////////////////////////////////////////
CircularQueue* RR_queue;
int RR_completed = 0;
int RR_pcbidx = 0;
struct PCB* RR_pcb;
bool RR_processRunning = false;
struct Process RR_runningProcess = {0,0,0,0,0,0,finished};
void RR(int process_count, int quantum)
{
/*
1-set handler to be sent when a process finishes
2-call circular queue to create the queue
3-while the queue is not empty
    1- check if there is a new process arrived and add it to the queue
    2- if the current running process is finished or this is the first process
        -fork and execl a process
        -set the PCB info of the current running process
    3- if the current running process is not finished
        -if yes, add it to the end of the queue and set its remaining time
        -if no, continue executing it until quantum expires or it finishes
4- when a process finishes, send a signal to the scheduler


 ----------------------corner case----------------------
1- if the process is not finished and the quantum expires, add it to the end of the queue
2- if the process is not finished and the quantum expires, but there are no other processes in the queue, continue executing it
3- if processes finish and quantum not finished 

*/



int RR_pid = -1;

    signal(SIGCHLD, RRhandler);  

    key_t msgkey = ftok("keyfile", 'p');
    int attach = msgget(msgkey, 0666 | IPC_CREAT);
    if (attach == -1) {
        perror("message queue not attached");
        return;
    }

    RR_queue = createCircularQueue(process_count);       
    RR_pcb = (struct PCB*)malloc(sizeof(PCB) * process_count);
    int pcbSize = 0;

    while (RR_completed < process_count) {               

        
        while (msgrcv(attach, &newMessage, sizeof(newMessage.p), 0, IPC_NOWAIT) != -1) {
            enqueueCircularQueue(RR_queue, newMessage.p);
            RR_pcb[pcbSize].p = newMessage.p;
            RR_pcb[pcbSize].remainingtime = newMessage.p.runtime;
            RR_pcb[pcbSize].paused = 0;
            RR_pcb[pcbSize].processstate = ready;
            pcbSize++;
        }

        // If process not running and queue is not empty, start next one
        if (!RR_processRunning && !isCircularQueueEmpty(RR_queue)) {
            RR_runningProcess = dequeueCircularQueue(RR_queue);
            RR_pcbidx = RR_runningProcess.id - 1;

            
            if (RR_pcb[RR_pcbidx].processstate == finished)
                continue;

            if (RR_pcb[RR_pcbidx].paused) {
                
                kill(RR_pcb[RR_pcbidx].current_pid, SIGCONT);
            } else {
                
                RR_pid = fork();
                if (RR_pid == 0) {
                    /*fprintf(logFile, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                        getClk(), RR_pcbidx, RR_pcb[RR_pcbidx].p.arrival_time, RR_pcb[RR_pcbidx].finishedtime, RR_pcb[RR_pcbidx].remainingtime, RR_pcb[RR_pcbidx].waitingtime);
                    */
                    char rt[10], id[10];
                    sprintf(rt, "%d", RR_pcb[RR_pcbidx].remainingtime);
                    sprintf(id, "%d", RR_runningProcess.id);
                    execl("./process.out", "./process.out", rt, id, NULL);
                    exit(1); // fail-safe
                }
                RR_pcb[RR_pcbidx].current_pid = RR_pid;
                RR_pcb[RR_pcbidx].executiontime = getClk();
                RR_pcb[RR_pcbidx].waitingtime = getClk() - RR_pcb[RR_pcbidx].p.arrival_time;
            }

            RR_pcb[RR_pcbidx].processstate = running;
            RR_processRunning = true;

            
            int startTime = getClk();
            while (getClk() - startTime < quantum) {
                int status;
                pid_t result = waitpid(RR_pcb[RR_pcbidx].current_pid, &status, WNOHANG);
                if (result != 0) {
                    
                    RR_processRunning = false;
                    break;
                }
            }

            
            if (RR_processRunning) {
                
                kill(RR_pcb[RR_pcbidx].current_pid, SIGSTOP);
                RR_pcb[RR_pcbidx].paused = 1;
                RR_pcb[RR_pcbidx].remainingtime -= quantum;
                RR_pcb[RR_pcbidx].processstate = ready;

        
                if (isCircularQueueEmpty(RR_queue)) {
                    kill(RR_pcb[RR_pcbidx].current_pid, SIGCONT);
                    int moreStartTime = getClk();
                    while (getClk() - moreStartTime < quantum) {
                        int status;
                        pid_t result = waitpid(RR_pcb[RR_pcbidx].current_pid, &status, WNOHANG);
                        if (result != 0) {
                            RR_processRunning = false;
                            break;
                        }
                    }

                    if (RR_processRunning) {
                        kill(RR_pcb[RR_pcbidx].current_pid, SIGSTOP);
                        RR_pcb[RR_pcbidx].paused = 1;
                        RR_pcb[RR_pcbidx].remainingtime -= quantum;
                        RR_pcb[RR_pcbidx].processstate = ready;
                        enqueueCircularQueue(RR_queue, RR_runningProcess);
                    }
                } else {
                    
                    enqueueCircularQueue(RR_queue, RR_runningProcess);
                }

                RR_processRunning = false;
            }
        }
    }

    
    destroyCircularQueue(RR_queue);
    free(RR_pcb);
    msgctl(attach, IPC_RMID, NULL);
    destroyClk(false);
}


void RRhandler(int sig_num) 
{
    RR_pcb[RR_pcbidx].finishedtime = getClk();
    RR_pcb[RR_pcbidx].processstate = finished;
    RR_runningProcess.processstate = finished;
    RR_completed++;
    kill(getppid(), SIGUSR1);
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


