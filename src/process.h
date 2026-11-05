#ifndef PROCESS_H
#define PROCESS_H

typedef struct
{
    int pid;
    int arrival;
    int burst;
    int priority;

    int remaining;  // used later by Round Robin / MLFQ
    int done;       // 1 once the process has finished
    int start_time; // first time it got the CPU
    int finish_time;
    int waiting;
    int turnaround;
    int response;
} Process;

#endif