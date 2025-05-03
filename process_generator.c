#include "headers.h"
#include "Defined_DS.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/wait.h>

// Function to clear resources on termination
void clearResources(int signum);
void ProcessCompleted(int signum);

// Global variables
int msgQid;
Process* processes;
int total_runtime = 0;
int scheduler_pid;
int clk_pid;
int CompletedByScheduler = 0;
int status = 0;

int main(int argc, char* argv[]) {

    signal(SIGINT, clearResources); // Handle termination signal
    
    // Validate input arguments
    if (argc < 1) {
        fprintf(stderr, "Usage: %s <processes_file> \n", argv[0]);
        return EXIT_FAILURE;
    }

    // Read input file to count the number of processes
    FILE* file = fopen(argv[1], "r");
    if (!file) {
        perror("Error opening processes.txt");
        return EXIT_FAILURE;
    }

    int process_count = 0;
    size_t len = 0;
    char* line = NULL;

    while (getline(&line, &len, file) != -1) {
        if (line[0] == '#') // Ignore comments
            continue;
        process_count++;
    }

    free(line);
    fclose(file);

    if (process_count == 0) {
        fprintf(stderr, "No valid processes found in input file.\n");
        return EXIT_FAILURE;
    }

    // Allocate memory for processes
    processes = (Process*)malloc(process_count * sizeof(Process));
    if (!processes) {
        perror("Error allocating memory for processes");
        return EXIT_FAILURE;
    }

    // Populate process array from the input file
    file = fopen(argv[1], "r");
    if (!file) {
        perror("Error reopening processes.txt");
        free(processes);
        return EXIT_FAILURE;
    }

    int i = 0;
    line = NULL;
    len = 0;

    while (getline(&line, &len, file) != -1) {
        if (line[0] == '#')
            continue;

        if (sscanf(line, "%d\t%d\t%d\t%d", 
                   &processes[i].id, 
                   &processes[i].arrival_time, 
                   &processes[i].runtime, 
                   &processes[i].priority) == 4) 
        {
            total_runtime += processes[i].runtime;
            i++;
        }
    }

    if (process_count > 0) {
        total_runtime += processes[0].arrival_time;
    }
    
    free(line);
    fclose(file);

    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    int algo_num;
    fprintf(stdout,"Please enter the algorithm you want to use ([1] Round Robin [2] SRTN [3] HPF):\n");
    scanf("%d",&algo_num);

    while (algo_num > 3 && algo_num < 1)
        fprintf(stdout,"Please enter a valid number([1] Round Robin [2] SRTN [3] HPF):\n");

    int quantum;

    if (algo_num == 1)
    {
        do
        {
            fprintf(stdout,"Please enter your preferred quantum:\n");
            scanf("%d",&quantum);
        }
        while (quantum < 0);
    }
    // 3. Initiate and create the scheduler and clock processes.   
    char quantum_str[10];
    if (algo_num == 1)
        sprintf(quantum_str, "%d",quantum);

    char process_count_str[10];
    sprintf(process_count_str, "%d", process_count);  

    char algo_num_str[10];
    sprintf(algo_num_str, "%d", algo_num);  

    scheduler_pid = fork();
    if (scheduler_pid == 0) {
        if (algo_num == 1)
            execl("./scheduler.out", "./scheduler.out", algo_num_str, process_count_str, quantum_str, NULL);
        else
            execl("./scheduler.out", "./scheduler.out", algo_num_str, process_count_str, NULL);
        perror("Scheduler execution failed");
        exit(EXIT_FAILURE);
    }

    clk_pid = fork();
    if (clk_pid == 0) {
        execl("./clk.out", "./clk.out", NULL);
        perror("Clock execution failed");
        exit(EXIT_FAILURE);
    }

    //Message queue setup  
    key_t msgkey = ftok("keyfile",'p');
    msgQid = msgget(msgkey, 0666 | IPC_CREAT);
    if (msgQid == -1) {
        perror("Failed to create message queue");
        free(processes);
        destroyClk(true);
        return EXIT_FAILURE;
    }

    // 4. Use this function after creating the clock process to initialize clock
    initClk();

    // 5. Create a data structure for processes and provide it with its parameters.
    struct msgbuff newMessage;

    // Step 6: Send processes to the scheduler at the appropriate time
    for (int i = 0; i < process_count; i++)
    {
        while(processes[i].arrival_time > getClk());

        newMessage.p = processes[i];
        if(msgsnd(msgQid,&newMessage,sizeof(newMessage.p),IPC_NOWAIT) == -1)
        {
            perror("Error sending process to scheduler");
        } else {
            printf("Sent Process %d to scheduler at time %d\n", processes[i].id,getClk());
        }

    }

    raise(SIGINT);
        
    return EXIT_SUCCESS;
}

void clearResources(int signum) 
{
    //TODO Clears all resources in case of interruption

    //wait for the scheduler to finish
    waitpid(scheduler_pid, &status, 0);

    printf("Cleaning up resources as Process generator...\n");

    msgctl(msgQid, IPC_RMID, NULL);
    
    if (processes)
        free(processes);
        
    // 7. Clear clock resources
    destroyClk(true);
    waitpid(clk_pid, &status, 0);

    exit(EXIT_SUCCESS);
}

void ProcessCompleted(int signum)
{
    CompletedByScheduler++;
    printf("sigusr1 received\n");
}
