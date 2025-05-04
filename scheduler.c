#include <math.h>
#include "headers.h"
#include "Defined_DS.h"
// -------------------Closing the scheduler---------------------
void closeScheduler(int sig_num);
// -------------------  Output file functions-------------------
void addToLog(int clk, int process_id,enum state state_num, int arrival,int runtime, int remaining_time, int wait_time,double TA, double WTA);
void addToPerf(struct PCB* pcb, int process_count);
// -------------------  HPF functions---------------------------
void HPF(int process_count);
void HPFhandler(int sig_num);
// -------------------  Round Robin functions-------------------
void RR(int process_count);
void RRhandler(int sig_num);
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
int quantum;
int main(int argc, char *argv[]) //1- alognumber,2- process_count,3- quantum
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
    quantum = atoi(argv[3]);
    if (logFile == NULL || perfFile == NULL) {
        perror("Error opening output files");
        exit(1);
    }
    quantum = atoi(argv[3]);
    
    int algo = atoi(argv[1]);
    int processes_count = atoi(argv[2]);

    printf("Scheduler PID: %d \n",getpid());

    switch (algo)
    {
        case 1:
            RR(atoi(argv[2]));
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
struct Process runningProcess = {0,0,0,0,0,0,finished};
int pcbidx = 0;     //tracks the current running process index

void HPF(int process_count)
{
      
    signal(SIGCHLD,HPFhandler);
    //tracks the number of processes that own a PCB  
    //tracks the number of processes that own a PCB  
    int pcbSize = 0;
    Process_queue = createPriorityQueue(process_count);
    pcb = (struct PCB*)malloc(sizeof(PCB) * process_count);
    int pid;
    //struct msgbuff newMessage;
    //initClk();
    while(completed < process_count)
    {
        if(msgrcv(attach,&newMessage,sizeof(newMessage.p),0,IPC_NOWAIT) != -1)
        {
            //printf("Process %d received at time %d\n",newMessage.p.id,getClk());
            insertPriorityPriorityQueue(Process_queue, newMessage.p);
            pcb[pcbSize].processstate = ready; //The test generator sets process ids from 1->process_count in order                                         
            pcb[pcbSize++].p = newMessage.p;    //We will access PCB of a certain process through its id
        }

        if (Process_queue->size > 0)
        {
            //if the current running process is finished or if this is the first process 
            if (pcb[pcbidx].processstate == finished || pcb[pcbidx].processstate == ready)   {
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
                    pcb[pcbidx].waitingtime = getClk() - runningProcess.arrival_time;
                    pcb[pcbidx].remainingtime = runningProcess.runtime;
                    pcb[pcbidx].executiontime = getClk();
                    pcb[pcbidx].processstate = running;
                    addToLog(getClk(),pcbidx + 1,running,runningProcess.arrival_time,runningProcess.runtime,runningProcess.runtime,pcb[pcbidx].waitingtime,-1,-1);
                }                  
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
    runningProcess.processstate = finished;
    completed++;
    addToLog(getClk(),pcbidx + 1,finished,runningProcess.arrival_time,runningProcess.runtime,0,pcb[pcbidx].waitingtime,pcb[pcbidx].TA,pcb[pcbidx].WTA);
    kill(getppid(),SIGUSR1);
    
}
//--------------------------------------------------------------------
//////////////////////////////////// Round Robin ///////////////////////////////////////////////////////
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

 ----------------------Corner case----------------------
1- if the process is not finished and the quantum expires, add it to the end of the queue
2- if the process is not finished and the quantum expires, but there are no other processes in the queue, continue executing it
3- if processes finish and quantum not finished 

*/
//////////////////////////////////// Round Robin ///////////////////////////////////////////////////////
CircularQueue* RR_queue;
int RR_completed = 0;
struct Process RR_runningProcess = {0,0,0,0,0,0,finished};
bool Run = false;
int RR_startTime = 0;
int RR_current_pid = -1;

void RR(int process_count) {
    
    RR_queue = createCircularQueue(process_count);
    RR_pcb = (struct PCB*)malloc(sizeof(PCB) * process_count);
    memset(RR_pcb, 0, sizeof(PCB) * process_count);
    
   // signal(SIGCHLD, RRhandler); 

    while (RR_completed < process_count) {
        
        int current_time = getClk();

        
        if(msgrcv(attach, &newMessage, sizeof(newMessage.p), 0, IPC_NOWAIT) != -1) {
            int idx = newMessage.p.id - 1;
            
            RR_pcb[idx].p = newMessage.p;
            RR_pcb[idx].remainingtime = newMessage.p.runtime;
            RR_pcb[idx].processstate = ready;
            printf("%d /n",RR_pcb[idx].p.id);
            enqueueCircularQueue(RR_queue, newMessage.p);
        }

        
        if (!isCircularQueueEmpty(RR_queue)) {
           
            
            if(Run && (current_time - RR_startTime >= quantum)) {
                kill(RR_current_pid, SIGSTOP);
                int t;
                pid_t result=waitpid(RR_pcb[pcbidx].current_pid,&t,WUNTRACED);
                if(WIFSTOPPED(t))
                {
                    
                        RR_runningProcess.processstate = stopped;
                        RR_pcb[pcbidx].remainingtime -= quantum;
                        addToLog(current_time, pcbidx + 1,
                                stopped,
                                RR_runningProcess.arrival_time,
                                RR_runningProcess.runtime,
                                RR_pcb[pcbidx].remainingtime,
                                current_time - RR_runningProcess.arrival_time - 
                                (RR_runningProcess.runtime - RR_pcb[pcbidx].remainingtime),
                                -1, -1);
                        enqueueCircularQueue(RR_queue, RR_runningProcess);
                    
                    Run = false;
                }else{
                    int pcbidx = RR_runningProcess.id - 1;
                    RR_pcb[pcbidx].finishedtime = getClk();
                    RR_pcb[pcbidx].processstate = finished;
                    RR_pcb[pcbidx].TA = RR_pcb[pcbidx].finishedtime - RR_runningProcess.arrival_time;
                    RR_pcb[pcbidx].WTA = RR_pcb[pcbidx].TA / RR_runningProcess.runtime;
                    RR_completed++;
                    Run = false;
                    
                    addToLog(RR_pcb[pcbidx].finishedtime, pcbidx + 1,
                            finished,
                            RR_runningProcess.arrival_time,
                            RR_runningProcess.runtime,
                            0,
                            RR_pcb[pcbidx].finishedtime - RR_runningProcess.arrival_time - RR_runningProcess.runtime,
                            RR_pcb[pcbidx].TA,
                            RR_pcb[pcbidx].WTA);
                    
                    kill(getppid(), SIGUSR1);
                }
               
            }
            
            if(!Run) {
                
                RR_runningProcess = dequeueCircularQueue(RR_queue);
                pcbidx = RR_runningProcess.id - 1;
                if(RR_pcb[pcbidx].processstate==stopped)
                {
                    RR_pcb[pcbidx].processstate = resumed;
                    kill(RR_pcb[pcbidx].current_pid,SIGCONT);
                    //RR_startTime = current_time;
                    printf("i resumed/n");

                }else{
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
                    RR_startTime = current_time;
                    Run = true;
                    
                    addToLog(current_time, pcbidx + 1,
                            running,
                            RR_runningProcess.arrival_time,
                            RR_runningProcess.runtime,
                            RR_pcb[pcbidx].remainingtime,
                            RR_pcb[pcbidx].waitingtime,
                            -1, -1);
                }
            }
            }
        
        }
    }
}

void RRhandler(int sig_num) {
   if( RR_runningProcess.processstate != running  && RR_runningProcess.processstate != resumed) {
    int pcbidx = RR_runningProcess.id - 1;
    RR_pcb[pcbidx].finishedtime = getClk();
    RR_pcb[pcbidx].processstate = finished;
    RR_pcb[pcbidx].TA = RR_pcb[pcbidx].finishedtime - RR_runningProcess.arrival_time;
    RR_pcb[pcbidx].WTA = RR_pcb[pcbidx].TA / RR_runningProcess.runtime;
    RR_completed++;
    Run = false;
    
    addToLog(RR_pcb[pcbidx].finishedtime, pcbidx + 1,
            finished,
            RR_runningProcess.arrival_time,
            RR_runningProcess.runtime,
            0,
            RR_pcb[pcbidx].finishedtime - RR_runningProcess.arrival_time - RR_runningProcess.runtime,
            RR_pcb[pcbidx].TA,
            RR_pcb[pcbidx].WTA);
    
    kill(getppid(), SIGUSR1);
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
   void addToLog(int clk, int process_id,enum state state_num, int arrival,int runtime, int remaining_time, int wait_time,double TA, double WTA)
   {
    
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
            fprintf(logFile, "At time %d process %d %s arr %d total %d remain %d wait %d TA %f WTA %.2f\n",clk,process_id,state,arrival,runtime,remaining_time,wait_time,TA,WTA);
            return;
            default:
            return;
        }

        fprintf(logFile, "At time %d process %d %s arr %d total %d remain %d wait %d\n",clk,process_id,state,arrival,runtime,remaining_time,wait_time);
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
