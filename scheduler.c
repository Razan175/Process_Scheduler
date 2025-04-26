#include "headers.h"
#include "Defined_DS.h"


PriorityQueue *Process_queue;

int completed;

void handler(int sig_num);

void HPF(int process_count);
int main(int argc, char *argv[])
{
    initClk();
//1-alognumber,2-process_count,3-quantum
    signal(SIGUSR1,handler);    
    int algo=atoi(argv[1]);
    
    //int algo_id = fork();
    printf("Scheduler PID: %d \n",getpid());
    //if (algo_id == 0)
    {
        switch (algo)
        {
            case 1:
                execl("./RR.out","./RR.out",argv[2],argv[3],NULL); //arv[2] = processes_count, argv[3] == quantum
            case 2:
                execl("./STRN.out","./STRN.out",argv[2],NULL);
            case 3:
                HPF(atoi(argv[2]));
            default:
                return EXIT_FAILURE;
        }
    }

    destroyClk(false);
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

    void HPF(int process_count)
    {
        key_t msgkey = ftok("keyfile",'p');
        int attach = msgget(msgkey, 0666 | IPC_CREAT);
        if(attach == -1)
        {
            perror("message queue not attached");
        }
        //signal(SIGCHLD,handler);

    
        Process_queue = createPriorityQueue(process_count);
        
        struct msgbuff newMessage;
        int t = 0 ,i = 0;
        struct PCB* pcb = (struct PCB*)malloc(sizeof(PCB) * process_count);
        initClk();
    
        while(completed < process_count)
        {
            //while (getClk() < t);
    
            int pid;
            if (Process_queue->size > 0)
            {
                if (Process_queue->array->processstate == ready)
                {
                    pid = fork();
                    if (pid == 0)
                    {
                        char* id;
                        
                        sprintf(id, "%d", Process_queue->array->runtime);
                        execl("./process.out","./process.out",id,NULL);
                    }
                    else
                    {
                        Process_queue->array->processstate == running;
                    }
                }
            }
           
            if (msgrcv(attach,&newMessage,sizeof(int),0,IPC_NOWAIT) != -1)
            {
                printf("%d\n",msgbuff.p);
                insertPriorityPriorityQueue(Process_queue, *(newMessage.p));
                pcb[i++].p = *(msgbuff.p);
            }
            else
            {
                perror("Error receiveing process to scheduler");
            }
    
            /*
                generally, if i got a signal that process is finished, remove from priqueue,
                state = finished
                
                3 cases:
                1- queue is empty, wait
                2- queue contains a running process, wait
                3- queue doesn't contain a running process, run the next process
            */
            t++;
        }
    
        free(pcb);
        destroyPriorityQueue(Process_queue);
        msgctl(attach,IPC_RMID,NULL);
    
        destroyClk(false);
    }

void handler(int sig_num)
{
    Process p = removePriorityPriorityQueue(Process_queue);
    completed++;
    p.processstate = finished;
    /*send sigusr1*/
    kill(getppid(),SIGUSR1);
}
