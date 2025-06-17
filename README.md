# **_Process Scheduler and Memory Management System_**
#### A single Core CPU scheduler Simulator that determines an order for the execution of its scheduled processes;
#### it decides which process will run according to the chosen algorithm and keeps track of the processes in the system and their status.

## **it supports 3 scheduling algorithms:**
#### 1- Non-Preemptive Highest Priority First (HPF)
#### 2- Round Robin (RR)
#### 3- Shortest Remaining time next (SRTN)

## **This helps users determine the most suitable algorithm for their needs by providing:**
#### 1- performance analytics (CPU Utilization, Average Wait time, Average Weighted Turnaround time)
#### 2- A log file that include the time each process starts, pauses,resumes and finishes

## It also includes memory allocation capabilities using the _buddy memory allocation system_. 
#### It allocates memory space for processes as they enter the system and free it as they leave so that it can be re-used by later processes.
