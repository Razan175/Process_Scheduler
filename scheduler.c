#include <math.h>
#include "headers.h"
#include "Defined_DS.h"

// ---------------- Closing the scheduler --------------------
void closeScheduler(int sig_num);

// ----------------  Output file functions -------------------
void addToLog(struct PCB pbcblock,int time);
void addToPerf(struct PCB* pcb, int process_count);
void MemLog(struct MemoryBlock mem, int time);

// -----------------  HPF functions --------------------------
void HPF(int process_count);
void HPFhandler(int sig_num);

// -----------------  RR functions ---------------------------
void RR(int process_count, int quantum);

// ----------------  SRTN functions --------------------------
void SRTN(int process_count);

//----------------- Memory Functions -------------------------
MemBlock* allocateMem(MemBlock* root, int size, int process_id);
bool freeMem(MemBlock* root, int process_id);

FILE *logFile;
FILE *perfFile;
FILE *MemFile;

key_t msgkey; 
int attach;

CircularQueue *BlockList;
struct PCB* pcb;
struct PCB* RR_pcb;

MemBlock* mem[4];

double util = 0;
int finish;

int main(int argc, char *argv[]) //1- alognumber,2- process_count,3- quantum
{
    
    initClk();
    signal(SIGINT,closeScheduler);

    msgkey = ftok("keyfile",'p');
    attach = msgget(msgkey, 0666 | IPC_CREAT);
    if(attach == -1)
    {
        perror("message queue not attached");
        exit(1);
    }

    logFile = fopen("scheduler.log", "w");
    perfFile = fopen("scheduler.perf", "w");
    MemFile = fopen("memory.log", "w");


    if (logFile == NULL || perfFile == NULL || MemFile == NULL) {
        perror("Error opening output files");
        msgctl(attach,IPC_RMID,NULL);
        exit(1);
    }
    
    fprintf(logFile,"#At time x process y state arr w total z remain y wait k\n");
    fprintf(MemFile,"#At time x allocated y bytes for process z from i to j\n");

    int algo = atoi(argv[1]);
    int processes_count = atoi(argv[2]);

    BlockList=createCircularQueue(processes_count);

    for (int i = 0, startB = 0; i < 4; i++, startB += 256)
        mem[i] = createMemBlock(startB,256,8);

    switch (algo)
    {
        case 1: 
            RR(processes_count,atoi(argv[3]));
            finish = getClk();
            addToPerf(RR_pcb,processes_count);
            break;
        case 2:
            SRTN(processes_count);
            break;
        case 3:
            HPF(processes_count); 
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
struct Process runningProcess = {0,0,0,0};
int pcbidx = 0;     //tracks the current running process index
int pausetime = 0;

void HPF(int process_count)
{
    completed = 0; 
    signal(SIGCHLD,HPFhandler);
    //tracks the number of processes that own a PCB  
    int pcbSize = 0;
    Process_queue = createPriorityQueue(process_count);
    pcb = (struct PCB*)malloc(sizeof(PCB) * process_count);
    int pid;

    while(completed < process_count)
    {
        int time = getClk();
        while(msgrcv(attach,&newMessage,sizeof(newMessage.p),0,IPC_NOWAIT) != -1 && time == getClk())
        {
            printf("Process %d received at time %d\n",newMessage.p.id,getClk());
            insertPriorityPriorityQueue(Process_queue, newMessage.p);
            pcb[pcbSize].processstate = ready;                  //The test generator sets process ids from 1->process_count in order                                         
            pcb[pcbSize].remainingtime = newMessage.p.runtime;  //We will access PCB of a certain process through its id
            pcb[pcbSize++].p = newMessage.p;  
        }

        if (Process_queue->size > 0)
        {
            //if the current running process is finished or if this is the first process 
            if (pcb[pcbidx].processstate == finished || pcb[pcbidx].processstate == ready)   {
                util += getClk() - pausetime;
                runningProcess = removePriorityPriorityQueue(Process_queue);
               
                MemBlock* allocate;
                int flag = 0;
                for (int i = 0; i < 4; i++)
                {
                    allocate = allocateMem(mem[i],runningProcess.memsize,runningProcess.id);
                    if (allocate){
                        flag = 1;
                        break;
                    }
                }
                if(!flag)
                {
                    enqueueCircularQueue(BlockList,runningProcess);
                    continue;
                }

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
                    runningProcess.memblock= *allocate;
                    pcb[pcbidx].memblock = allocate;
                    addToLog(pcb[pcbidx],getClk());
                }                  
            }
        }
    }
}

void HPFhandler(int sig_num)
{
    //sets the needed pcb info of the finished process
    
    pausetime = getClk();
    pcb[pcbidx].remainingtime = 0;
    pcb[pcbidx].finishedtime = getClk();
    pcb[pcbidx].processstate = finished;
    pcb[pcbidx].TA = pcb[pcbidx].finishedtime - runningProcess.arrival_time ;
    pcb[pcbidx].WTA = pcb[pcbidx].TA/runningProcess.runtime;
    completed++;
    addToLog(pcb[pcbidx],getClk());  
    
    bool isfree = freeMem(mem[0],runningProcess.id);
    if(!isfree)
    {
        printf("Error freeing memory for process %d\n",runningProcess.id);
    }
    int blockcount = BlockList->size;
    for(int i=0;i<blockcount;i++)
    {
        struct Process blockedProcess=dequeueCircularQueue(BlockList);
        int memsize=blockedProcess.memsize;
        MemBlock* allocate=allocateMem(mem[0],memsize,blockedProcess.id);
        if(allocate)
        {
            MemLog(*allocate,getClk());
            blockedProcess.memblock=*allocate;
            insertPriorityPriorityQueue(Process_queue,blockedProcess);
            break;
        }
        else
        {
            enqueueCircularQueue(BlockList,blockedProcess);
            break;
        }
            
    }   
}


//////////////////////////////////// Round Robin ///////////////////////////////////////////////////////
CircularQueue* RR_queue;
int RR_completed = 0;
struct Process RR_runningProcess = {0,0,0,0,0};
bool Run = false;
int RR_startTime = 0;

void RR(int process_count, int quantum) {   
    RR_queue = createCircularQueue(process_count);
    RR_pcb = (struct PCB*)malloc(sizeof(PCB) * process_count);
    memset(RR_pcb, 0, sizeof(PCB) * process_count);

    while (RR_completed < process_count) {

        while (msgrcv(attach, &newMessage, sizeof(newMessage.p), 0, IPC_NOWAIT) != -1) {
            int idx = newMessage.p.id - 1;       
            RR_pcb[idx].p = newMessage.p;
            RR_pcb[idx].remainingtime = newMessage.p.runtime;
            RR_pcb[idx].processstate = ready;
            enqueueCircularQueue(RR_queue, newMessage.p);
        }

        int current_time = getClk();
    
        if(Run && (current_time - RR_startTime >= quantum)) {
            if (RR_pcb[pcbidx].remainingtime > quantum)
                kill(RR_pcb[pcbidx].current_pid, SIGSTOP);
            

            int t;
            pausetime = getClk();
            pid_t result = waitpid(RR_pcb[pcbidx].current_pid,&t,WUNTRACED);
            if(WIFSTOPPED(t))
            {
                current_time = getClk();
                RR_pcb[pcbidx].processstate = stopped;
                RR_pcb[pcbidx].remainingtime -= quantum;
                addToLog(RR_pcb[pcbidx], getClk());
                enqueueCircularQueue(RR_queue, RR_runningProcess);       
                Run = false;
            }
            else 
            {
                RR_pcb[pcbidx].finishedtime = getClk();
                RR_pcb[pcbidx].processstate = finished;
                RR_pcb[pcbidx].TA = RR_pcb[pcbidx].finishedtime - RR_runningProcess.arrival_time;
                RR_pcb[pcbidx].WTA = RR_pcb[pcbidx].TA / RR_runningProcess.runtime;
                RR_completed++;
                RR_pcb[pcbidx].remainingtime = 0;
                Run = false; 
                bool allocate = false;
                for (int i = 0; i < 4; i++)
                {
                    allocate = freeMem(mem[i],RR_runningProcess.id);
                    if (allocate)
                        break;
                }
                addToLog(RR_pcb[pcbidx],getClk());    
            }
        }

        if (!isCircularQueueEmpty(BlockList) && !Run)
        {
            MemBlock* allocate;
            int flag = 0;
            for (int i = 0; i < 4; i++)
            {
                allocate = allocateMem(mem[i],BlockList->array->memsize,BlockList->array->id);
                    if (allocate != NULL){
                        flag = 1;
                        break;
                    }
                }
                if(flag)
                {
                    RR_runningProcess = dequeueCircularQueue(BlockList);
                    pcbidx = RR_runningProcess.id - 1;
                    if(RR_pcb[pcbidx].remainingtime > quantum)
                        RR_startTime = current_time;
                    else
                        RR_startTime = current_time - (quantum - RR_pcb[pcbidx].remainingtime);

                    util += getClk() - pausetime;  

                    RR_pcb[pcbidx].current_pid = fork();
                    if (RR_pcb[pcbidx].current_pid == 0) {
                        char rt[10], id[10];
                        sprintf(rt, "%d", RR_pcb[pcbidx].remainingtime);
                        sprintf(id, "%d", RR_runningProcess.id);
                        execl("./process.out", "./process.out", rt, id, NULL);
                    }
                    else {
                        RR_pcb[pcbidx].waitingtime = current_time - RR_runningProcess.arrival_time;
                        RR_pcb[pcbidx].processstate = running;
                        RR_pcb[pcbidx].memblock = allocate;
                        Run = true;
                        addToLog(RR_pcb[pcbidx],getClk());
                    }
                }
        } 

        if (!isCircularQueueEmpty(RR_queue) && !Run) {
         
            RR_runningProcess = dequeueCircularQueue(RR_queue);
            pcbidx = RR_runningProcess.id - 1;
            current_time = getClk();
            if(RR_pcb[pcbidx].remainingtime > quantum)
                RR_startTime = current_time;
            else
                RR_startTime = current_time - (quantum - RR_pcb[pcbidx].remainingtime);

                
            if (RR_pcb[pcbidx].processstate == stopped)
            {
                util += getClk() - pausetime; 
                Run = true;
                RR_pcb[pcbidx].processstate = resumed;
                kill(RR_pcb[pcbidx].current_pid,SIGCONT);
                addToLog(RR_pcb[pcbidx],getClk());      
            }
            else
            {
                MemBlock* allocate;
                int flag = 0;
                for (int i = 0; i < 4; i++)
                {
                    allocate = allocateMem(mem[i],RR_runningProcess.memsize,RR_runningProcess.id);
                    if (allocate != NULL){
                        flag = 1;
                        break;
                    }
                }
               if(!flag)
                {
                    enqueueCircularQueue(BlockList,RR_runningProcess);
                    continue;
                }
                RR_pcb[pcbidx].current_pid = fork();

                if (RR_pcb[pcbidx].current_pid == 0) {
                    char rt[10], id[10];
                    sprintf(rt, "%d", RR_pcb[pcbidx].remainingtime);
                    sprintf(id, "%d", RR_runningProcess.id);
                    execl("./process.out", "./process.out", rt, id, NULL);
                }
                else 
                {
                    RR_pcb[pcbidx].waitingtime = current_time - RR_runningProcess.arrival_time;
                    RR_pcb[pcbidx].processstate = running;
                    Run = true;
                    util += getClk() - pausetime; 
                    RR_pcb[pcbidx].memblock = allocate;

                    addToLog(RR_pcb[pcbidx],getClk());
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


//----------------------allocate memory block----------------------
MemBlock* allocateMem(MemBlock* root, int size, int process_id)
{ 
    if(!root || !(root->isFree) || root->bytes < size || size < 2)
        return NULL;
    
    int nearestPower = (int)(ceil(log2(size)));

    //if size is equal to block no need to split
    if(root->power == nearestPower && root->isFree && !root->isSplit)
    {
        root->isFree = false;
        root->process_id = process_id;
        MemLog(*root,getClk());
        return root;
    }
    // if not and the block was never split before
    if(!(root->isSplit) && root->isFree)
    {
        if(!root->left)
            root->left = createMemBlock(root->start, root->bytes/2, root->power - 1);
        if(!root->right)
            root->right = createMemBlock((root->left->end) + 1, root->bytes/2, root->power - 1);
        root->isSplit = true;
    }

    // where to allocate check left first is free allocate no see right
    MemBlock* found = allocateMem(root->left, size, process_id);
    if(!found)
        found = allocateMem(root->right, size, process_id);

    //when found recursively set isSplit to true
    if(found)
        root->isSplit = true;

    return found;
}

//----------------------Free memory block----------------------
bool freeMem(MemBlock* root, int process_id) {

    if (root == NULL) 
        return false;

    //if found
    if (root->process_id == process_id) {
        root->isFree = true;
        root->process_id = -1;
        MemLog(*root,getClk());
        return true;
    }
    //Traverse the tree in case the block is not found
    bool leftFreed = freeMem(root->left, process_id);
    bool rightFreed = freeMem(root->right, process_id);

    //recursively checking isSplit (ex if 32 is freed, we must check if its parent 64 is still split then check if 128 is still split and so on
    if (leftFreed || rightFreed)
    {
        if (((root->right && root->right->isFree) || root->right == NULL) && ((root->left && root->left->isFree )|| (root->right == NULL)))
            root->isSplit = false;
    }
    return leftFreed || rightFreed;
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
        break;
        default:
        return;
    }

    fprintf(logFile, "At time %d process %d %s arr %d total %d remain %d wait %d",time,process_id,state,arrival,runtime,remaining_time,wait_time);
    
    if (state == "Finished")
        fprintf(logFile, " TA %.0f WTA %.2f",TA,WTA);   
    
    fprintf(logFile,"\n");
}
void MemLog(MemBlock mem, int time)
{
    //#At time x allocated y bytes for process z from i to j
     int size = mem.bytes;
     int start = mem.start;
     int end = mem.end;
     int process_id = mem.process_id;
   
     if (mem.isFree)
     {
         fprintf(MemFile, "At time %d freed %d bytes from %d to %d\n", time, size, start, end);
     }
     else
     {
         fprintf(MemFile, "At time %d allocated %d bytes for process %d from %d to %d\n", time, size, process_id, start, end);
     }
}

void addToPerf(struct PCB *pcb, int process_count)
{
    
    double avgWait = 0, avgWTA = 0, stdWTA = 0,totalexc=0 ;
    int arrive;
    int totalrun;
    finish = pcb[0].finishedtime;
    arrive = pcb[0].p.arrival_time;
    for(int i = 0; i < process_count; i++)
    {
        
        avgWait += pcb[i].waitingtime;
        totalexc +=pcb[i].p.runtime;
        avgWTA += pcb[i].WTA;

        if(pcb[i].finishedtime>finish)
            finish=pcb[i].finishedtime;
    }

    avgWait /= (double)process_count;
    avgWTA /= (double)process_count;

    for(int i = 0; i < process_count; i++)
        stdWTA += pow(pcb[i].WTA - avgWTA, 2);

    stdWTA /= (double)process_count;
    stdWTA = sqrt(stdWTA);

    util = (1 - (util/finish))*100;

    fprintf(perfFile,"CPU utilization = %.2f%%\nAvg WTA = %.2f\nAvg Waiting = %.2f\nStd WTA = %.2f\n",util,avgWTA,avgWait,stdWTA);
}

void closeScheduler(int sig_num)
{
    printf("Cleaning up resources as Scheduler\n");

    for (int i = 0; i < 4; i++)
        if(mem[i])
            free(mem[i]);

    if(logFile)
        fclose(logFile);
    if(perfFile)
        fclose(perfFile);
    if(MemFile)
        fclose(MemFile);

    if(pcb)
        free(pcb);

    destroyPriorityQueue(Process_queue);

    if(attach != -1)
        msgctl(attach,IPC_RMID,NULL);
        
    destroyClk(false);
    
    destroyCircularQueue(RR_queue);
    destroyCircularQueue(BlockList);
    if(RR_pcb)
         free(RR_pcb);

    exit(0);
}