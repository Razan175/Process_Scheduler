#include <math.h>
#include "headers.h"
#include "Defined_DS.h"
// -------------------Closing the scheduler---------------------
void closeScheduler(int sig_num);
// -------------------  Output file functions-------------------
void addToLog(struct PCB pbcblock,int time);
void addToPerf(struct PCB* pcb, int process_count);
// -------------------  HPF functions---------------------------
void HPF(int process_count);
void HPFhandler(int sig_num);
// -------------------  Round Robin functions-------------------
void RR(int process_count);
// -------------------  SRTN functions---------------------------
void SRTN(int process_count);
//---------------------------------------------------------------------------------------------------------------------------------------------
FILE *logFile;
FILE *perfFile;
key_t msgkey; 
int attach;
struct PCB* pcb;
struct PCB* RR_pcb;
int quantum;
int main(int argc, char *argv[]) //1- alognumber, 2- process_count, 3- quantum
{
    initClk();
    signal(SIGINT,closeScheduler);

    msgkey = ftok("keyfile",'p');
    attach = msgget(msgkey, 0666 | IPC_CREAT);
    if(attach == -1)
    {
        perror("message queue not attached");
    }

    logFile = fopen("scheduler.log", "w");
    perfFile = fopen("scheduler.perf", "w");

    if (logFile == NULL || perfFile == NULL) {
        perror("Error opening output files");
        exit(1);
    }

    
    int algo = atoi(argv[1]);
    int processes_count = atoi(argv[2]);

    printf("Scheduler PID: %d \n",getpid());

    switch (algo)
    {
        case 1:
            quantum = atoi(argv[3]);  
            RR(atoi(argv[2]));
            addToPerf(RR_pcb,processes_count);
            break;
        case 2:
            SRTN(atoi(argv[2]));
            break;
        case 3:
            HPF(atoi(argv[2])); 
            addToPerf(pcb,processes_count);
            break;
        default:
            return EXIT_FAILURE;
    }

    raise(SIGINT);
}

////////////////////////////////////////// Highest Priority first ////////////////////////////////////////

PriorityQueue *Process_queue;
int completed;
struct Process runningProcess = {0,0,0,0,0,0};
int pcbidx = 0;     //tracks the current running process index

void HPF(int process_count)
{
      
    signal(SIGCHLD,HPFhandler);
    //tracks the number of processes that own a PCB  
    int pcbSize = 0;
    Process_queue = createPriorityQueue(process_count);
    pcb = (struct PCB*)malloc(sizeof(PCB) * process_count);
    int pid;

    while(completed < process_count)
    {
        while(msgrcv(attach,&newMessage,sizeof(newMessage.p),0,IPC_NOWAIT) != -1)
        {
            printf("Process %d received at time %d\n",newMessage.p.id,getClk());
            insertPriorityPriorityQueue(Process_queue, newMessage.p);
            pcb[pcbSize].processstate = ready; //The test generator sets process ids from 1->process_count in order                                         
            pcb[pcbSize].remainingtime = newMessage.p.runtime; 
            pcb[pcbSize++].p = newMessage.p;    //We will access PCB of a certain process through its id
        }

        if (Process_queue->size > 0)
        {
            //if the current running process is finished or if this is the first process 
            if (pcb[pcbidx].processstate == finished || pcb[pcbidx].processstate == ready)   {
                runningProcess = removePriorityPriorityQueue(Process_queue);
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
                    pcb[pcbidx].waitingtime = getClk() - runningProcess.arrival_time;
                    pcb[pcbidx].remainingtime = runningProcess.runtime;
                    pcb[pcbidx].executiontime = getClk();
                    pcb[pcbidx].processstate = running;
                    addToLog(pcb[pcbidx],getClk());
                }                  
            }
        }
    }
}

void HPFhandler(int sig_num)
{
    //sets the needed pcb info of the finished process
    pcb[pcbidx].finishedtime = getClk();
    pcb[pcbidx].processstate = finished;
    pcb[pcbidx].TA = pcb[pcbidx].finishedtime - runningProcess.arrival_time ;
    pcb[pcbidx].WTA = pcb[pcbidx].TA/runningProcess.runtime;
    completed++;
    addToLog(pcb[pcbidx],getClk());
    kill(getppid(),SIGUSR1);
    
}


//////////////////////////////////// Round Robin ///////////////////////////////////////////////////////
CircularQueue* RR_queue;
int RR_completed = 0;
struct Process RR_runningProcess = {0,0,0,0,0,0};
bool Run = false;
int RR_startTime = 0;
int RR_current_pid = -1;

void RR(int process_count) {
    
    RR_queue = createCircularQueue(process_count);
    RR_pcb = (struct PCB*)malloc(sizeof(PCB) * process_count);
    memset(RR_pcb, 0, sizeof(PCB) * process_count);
    

    while (RR_completed < process_count) {
        
        int current_time = getClk();

        
        if(msgrcv(attach, &newMessage, sizeof(newMessage.p), 0, IPC_NOWAIT) != -1) {
            int idx = newMessage.p.id - 1;
            
            RR_pcb[idx].p = newMessage.p;
            RR_pcb[idx].remainingtime = newMessage.p.runtime;
            RR_pcb[idx].processstate = ready;
            enqueueCircularQueue(RR_queue, newMessage.p);
        }
            if(Run && (current_time - RR_startTime >= quantum)) {
               if (RR_pcb[pcbidx].remainingtime > quantum)
                    kill(RR_current_pid, SIGSTOP);
                int t;
                pid_t result = waitpid(RR_pcb[pcbidx].current_pid,&t,WUNTRACED);
                if(WIFSTOPPED(t))
                {
                        current_time = getClk();
                        RR_pcb[pcbidx].processstate = stopped;
                        RR_pcb[pcbidx].remainingtime -= quantum;
                        addToLog(RR_pcb[pcbidx], getClk());
                        enqueueCircularQueue(RR_queue, RR_runningProcess);       
                    Run = false;
                }else{
                    RR_pcb[pcbidx].finishedtime = getClk();
                    RR_pcb[pcbidx].processstate = finished;
                    RR_pcb[pcbidx].TA = RR_pcb[pcbidx].finishedtime - RR_runningProcess.arrival_time;
                    RR_pcb[pcbidx].WTA = RR_pcb[pcbidx].TA / RR_runningProcess.runtime;
                    RR_completed++;
                    RR_pcb[pcbidx].remainingtime = 0;
                    Run = false;
                    
                    addToLog(RR_pcb[pcbidx],getClk());
                    
                    kill(getppid(), SIGUSR1);
                }
            }
            if (!isCircularQueueEmpty(RR_queue)) {
                if(!Run) {
                    current_time = getClk();
                    RR_runningProcess = dequeueCircularQueue(RR_queue);
                    pcbidx = RR_runningProcess.id - 1;
                    Run = true;
                    if(RR_pcb[pcbidx].remainingtime > quantum)
                        RR_startTime = current_time;
                    else
                        RR_startTime = current_time - (quantum - RR_pcb[pcbidx].remainingtime);

                    if(RR_pcb[pcbidx].processstate == stopped)
                    {
                        printf("i resumed/n");
                        RR_pcb[pcbidx].processstate = resumed;
                        kill(RR_pcb[pcbidx].current_pid,SIGCONT);
                        addToLog(RR_pcb[pcbidx],getClk());      
                    }
                    else
                    {
                        RR_current_pid = fork();
                        if (RR_current_pid == 0) {
                            char rt[10], id[10];
                            sprintf(rt, "%d", RR_pcb[pcbidx].remainingtime);
                            sprintf(id, "%d", RR_runningProcess.id);
                            execl("./process.out", "./process.out", rt, id, NULL);
                        }
                        else {
                        RR_pcb[pcbidx].waitingtime = current_time - RR_runningProcess.arrival_time;
                        RR_pcb[pcbidx].processstate = running;
                    
                        addToLog(RR_pcb[pcbidx],getClk());
                    }
                }
                }
            }
    
    }
}

//////////////////////////////////// Shortest Remaining Time Next //////////////////////////////////////
void SRTN(int process_count)
{
    /*
        1- read msg queue
        2- add process to priority queue
        3- each second, the process sends a signal to decrement its remaining time
        4- check if the process at the face of the queue is the same as the currently running process 
            -if no, pause this process and save its pid somehow (we can add a pid to PCB)
        5- when a process is done, remove it from the queue and go to the next process
        6- if the next process was never forked before, fork it, else resume it
    */
}
   void addToLog(struct PCB pcblock, int time)
   {
        int process_id = pcblock.p.id;
        enum state state_num = pcblock.processstate;
        int arrival = pcblock.p.arrival_time;
        int runtime = pcblock.p.runtime;
        int remaining_time = pcblock.remainingtime;
        int wait_time = pcblock.waitingtime; 
        double TA = pcblock.TA;
        double WTA =  pcblock.WTA;
        char* state;
        switch (state_num)
        {
            case running:
            state = "Started";
            break;
            case stopped:
            state = "Stopped";
            break;
            case resumed:
            state = "Resumed";
            break;
            case finished:
            state = "Finished";
            fprintf(logFile, "At time %d process %d %s arr %d total %d remain %d wait %d TA %f WTA %.2f\n",time,process_id,state,arrival,runtime,remaining_time,wait_time,TA,WTA);
            return;
            default:
            return;
        }

        fprintf(logFile, "At time %d process %d %s arr %d total %d remain %d wait %d\n",time,process_id,state,arrival,runtime,remaining_time,wait_time);
       // fwrite(data,sizeof(data),sizeof(data)/sizeof(data[0]),logFile);)
   }

   void addToPerf(struct PCB *pcb, int process_count)
   {

    double avgWait = 0, avgWTA = 0, stdWTA = 0;

    for(int i = 0; i < process_count; i++)
    {
        avgWait += pcb[i].waitingtime;
        avgWTA += pcb[i].WTA;
    }

    avgWait /= (double)process_count;
    avgWTA /= (double)process_count;

    for(int i = 0; i < process_count; i++)
        stdWTA += pow(pcb[i].WTA - avgWTA, 2);

    stdWTA /= (double)process_count;
    stdWTA = sqrt(stdWTA);

    fprintf(perfFile,"Avg WTA = %.2f\nAvg Waiting = %.2f\nStd WTA = %.2f\n",avgWTA,avgWait,stdWTA);
   }

   void closeScheduler(int sig_num)
   {
       if(logFile)
           fclose(logFile);
       if(perfFile)
           fclose(perfFile);
       if(pcb)
           free(pcb);
   
       destroyPriorityQueue(Process_queue);
   
       //if(attach != -1)
         //  msgctl(attach,IPC_RMID,NULL);
       destroyClk(false);
       
       destroyCircularQueue(RR_queue);

       if(RR_pcb)
            free(RR_pcb);
   }
