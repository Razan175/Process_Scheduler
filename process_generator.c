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
char* File_name;
int total_runtime = 0;
int scheduler_pid;
int clk_pid;
int CompletedByScheduler = 0;


int main(int argc, char* argv[]) {

    // ✅ Validate input arguments
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <processes_file> <algorithm_number> [quantum]\n", argv[0]);
        return EXIT_FAILURE;
    }

    signal(SIGINT, clearResources); // Handle termination signal
    signal(SIGUSR1, ProcessCompleted);

    // Correct memory allocation for File_name
    File_name = strdup(argv[1]);
    if (!File_name) {
        perror("Failed to allocate memory for File_name");
        return EXIT_FAILURE;
    }

    // Step 1: Read input file to count the number of processes
    FILE* file = fopen(File_name, "r");
    if (!file) {
        perror("Error opening processes.txt");
        free(File_name);
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
        free(File_name);
        return EXIT_FAILURE;
    }

    // Step 2: Allocate memory for processes
    processes = (Process*)malloc(process_count * sizeof(Process));
    if (!processes) {
        perror("Error allocating memory for processes");
        free(File_name);
        return EXIT_FAILURE;
    }

    // Step 3: Populate process array from the input file
    file = fopen(File_name, "r");
    if (!file) {
        perror("Error reopening processes.txt");
        free(processes);
        free(File_name);
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
            processes[i].prempted = false;
            total_runtime += processes[i].runtime;
            i++;
        }
        printf("%d\t%d\t%d\t%d\t", 
                   processes[i - 1].id, 
                   processes[i - 1].arrival_time, 
                   processes[i - 1].runtime, 
                   processes[i - 1].priority);
                printf("\n");

    }

    //if (process_count > 0) {
      //  total_runtime += processes[0].arrival_time;
    //}

    free(line);
    fclose(file);

    // Step 4: Start clock and scheduler processes
    int algo_num = atoi(argv[2]);
    char quantum_str[10] = "";
    if (argc > 3)
        sprintf(quantum_str, "%d", atoi(argv[3]));

    clk_pid = fork();
    if (clk_pid == 0) {
        execl("./clk.out", "./clk.out", NULL);
        perror("Clock execution failed");
        exit(EXIT_FAILURE);
    }

    sleep(1);
    initClk();

    char process_count_str[10];
    sprintf(process_count_str, "%d", process_count); //copy any thing to string 

    scheduler_pid = fork();
    if (scheduler_pid == 0) {
        if (argc > 3)
            execl("./scheduler.out", "./scheduler.out", argv[2], process_count_str, quantum_str, NULL);
        else
            execl("./scheduler.out", "./scheduler.out", argv[2], process_count_str, NULL);
        perror("Scheduler execution failed");
        exit(EXIT_FAILURE);
    }
// need to change ipc_private
    // Step 5: Message queue setup  
    key_t msgkey = ftok("keyfile",'p');
    msgQid = msgget(msgkey, 0666 | IPC_CREAT);
    if (msgQid == -1) {
        perror("Failed to create message queue");
        free(processes);
        free(File_name);
        destroyClk(true);
        return EXIT_FAILURE;
    }

    
    // Step 6: Send processes to the scheduler at the appropriate time
    /*for (int t = 0, sent = 0; sent < process_count; t++) {
        while (getClk() < t); // Wait for clock sync

        for (int j = 0; j < process_count; j++) {
            if (processes[j].arrival_time == t) {
                
                newMessage.p = processes[j];

                if (msgsnd(msgQid, &newMessage, sizeof(newMessage), 0) == -1) {
                    perror("Error sending process to scheduler");
                } else {
                    printf("Sent Process %d to scheduler at time %d\n", processes[j].id, t);
                    sent++;
                }
            }
        }
    }
*/
    // Step 6: Send processes to the scheduler at the appropriate time
    for (int i = 0; i < process_count; i++)
    {
        while( processes[i].arrival_time != getClk());
        struct msgbuff msg;
        msg.p = &processes[i];
        if(msgsnd(msgQid,&msg,sizeof(msg.p),!IPC_NOWAIT) == -1)
        {
            perror("Error sending process to scheduler");
        } else {
            printf("Sent Process %d to scheduler at time %d\n", processes[i].id,getClk());
        }

    }
    /*
    for each proces
        1-while(scan arrival time[i] != getclk());
        2-send in msgqueue
        3-printf proccess sent at time t
    */

    while (CompletedByScheduler < process_count) {}

    printf("All processes have been sent and completed!\nShutting down Process Generator and Scheduler.\n");
    raise(SIGINT);
    


    return EXIT_SUCCESS;
}

void clearResources(int signum) 
{
    printf("Cleaning up resources as Process generator...\n");
    msgctl(msgQid, IPC_RMID, NULL);
    if (processes)
        free(processes);
    if (File_name)
        free(File_name);

    kill(scheduler_pid, SIGINT);
    int status;
    waitpid(scheduler_pid, &status, 0);

    destroyClk(true);
    waitpid(clk_pid, &status, 0);

    exit(EXIT_SUCCESS);
}

void ProcessCompleted(int signum)
{
    CompletedByScheduler++;
    printf("sigusr1 received\n");
}
