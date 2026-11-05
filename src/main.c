#include <stdio.h>
#include "process.h"
#define LEVELS 3
#define QSIZE 100

typedef struct
{
    int items[QSIZE];
    int head, tail, count;
} Queue;

void q_push(Queue *q, int value)
{
    q->items[q->tail] = value;
    q->tail = (q->tail + 1) % QSIZE;
    q->count++;
}

int q_pop(Queue *q)
{
    int value = q->items[q->head];
    q->head = (q->head + 1) % QSIZE;
    q->count--;
    return value;
}

void sort_by_arrival(Process p[], int n)
{
    for (int i = 1; i < n; i++)
    {
        Process key = p[i];
        int j = i - 1;
        while (j >= 0 && p[j].arrival > key.arrival)
        {
            p[j + 1] = p[j];
            j--;
        }
        p[j + 1] = key;
    }
}

void reset_processes(Process p[], int n)
{
    for (int i = 0; i < n; i++)
    {
        p[i].remaining = p[i].burst;
        p[i].start_time = 0;
        p[i].finish_time = 0;
        p[i].waiting = 0;
        p[i].turnaround = 0;
        p[i].response = 0;
        p[i].done = 0;
    }
}

void run_fcfs(Process p[], int n)
{
    sort_by_arrival(p, n);
    reset_processes(p, n);

    int time = 0;
    for (int i = 0; i < n; i++)
    {
        if (time < p[i].arrival)
        {
            time = p[i].arrival; // CPU sits idle until it arrives
        }
        p[i].start_time = time;
        time += p[i].burst;
        p[i].finish_time = time;

        p[i].turnaround = p[i].finish_time - p[i].arrival;
        p[i].waiting = p[i].turnaround - p[i].burst;
        p[i].response = p[i].start_time - p[i].arrival;
    }
}

void run_sjf(Process p[], int n)
{
    reset_processes(p, n);

    int time = 0;
    int completed = 0;

    while (completed < n)
    {
        // pick the shortest job among those that have already arrived
        int pick = -1;
        for (int i = 0; i < n; i++)
        {
            if (p[i].done || p[i].arrival > time)
            {
                continue;
            }
            if (pick == -1 || p[i].burst < p[pick].burst)
            {
                pick = i;
            }
        }

        if (pick == -1)
        {
            // nothing has arrived yet, jump to the earliest unfinished arrival
            int next = -1;
            for (int i = 0; i < n; i++)
            {
                if (!p[i].done && (next == -1 || p[i].arrival < p[next].arrival))
                {
                    next = i;
                }
            }
            time = p[next].arrival;
            continue;
        }

        p[pick].start_time = time;
        time += p[pick].burst;
        p[pick].finish_time = time;
        p[pick].turnaround = p[pick].finish_time - p[pick].arrival;
        p[pick].waiting = p[pick].turnaround - p[pick].burst;
        p[pick].response = p[pick].start_time - p[pick].arrival;
        p[pick].done = 1;
        completed++;
    }
}

int run_rr(Process p[], int n, int quantum)
{
    sort_by_arrival(p, n);
    reset_processes(p, n);

    int queue[100]; // circular ready queue holding indexes into p[]
    int head = 0, tail = 0, count = 0;

    for (int i = 0; i < n; i++)
    {
        p[i].start_time = -1; // -1 means "has not run yet"
    }

    int time = 0;
    int completed = 0;
    int next_arrival = 0; // next process (in arrival order) not yet queued
    int switches = 0;
    int last_run = -1;

    while (completed < n)
    {
        // move everyone who has arrived by now into the ready queue
        while (next_arrival < n && p[next_arrival].arrival <= time)
        {
            queue[tail] = next_arrival;
            tail = (tail + 1) % 100;
            count++;
            next_arrival++;
        }

        if (count == 0)
        {
            // CPU idle, jump to the next arrival
            time = p[next_arrival].arrival;
            continue;
        }

        int cur = queue[head];
        head = (head + 1) % 100;
        count--;

        if (p[cur].start_time == -1)
        {
            p[cur].start_time = time;
            p[cur].response = time - p[cur].arrival;
        }

        if (last_run != -1 && last_run != cur)
        {
            switches++;
        }

        int slice = quantum;
        if (p[cur].remaining < slice)
        {
            slice = p[cur].remaining;
        }
        time += slice;
        p[cur].remaining -= slice;

        // processes that arrived during this slice go in BEFORE the preempted one
        while (next_arrival < n && p[next_arrival].arrival <= time)
        {
            queue[tail] = next_arrival;
            tail = (tail + 1) % 100;
            count++;
            next_arrival++;
        }

        if (p[cur].remaining == 0)
        {
            p[cur].finish_time = time;
            p[cur].turnaround = p[cur].finish_time - p[cur].arrival;
            p[cur].waiting = p[cur].turnaround - p[cur].burst;
            p[cur].done = 1;
            completed++;
        }
        else
        {
            queue[tail] = cur;
            tail = (tail + 1) % 100;
            count++;
        }

        last_run = cur;
    }

    return switches;
}

void print_report(Process p[], int n, const char *name)
{
    int total_busy = 0, last_finish = 0;
    double sum_wait = 0, sum_tat = 0;

    printf("\n=== %s ===\n", name);
    printf("PID  Arr  Burst  Start  Finish  Wait  TAT  Resp\n");
    for (int i = 0; i < n; i++)
    {
        printf("%-4d %-4d %-6d %-6d %-7d %-5d %-4d %-4d\n",
               p[i].pid, p[i].arrival, p[i].burst, p[i].start_time,
               p[i].finish_time, p[i].waiting, p[i].turnaround, p[i].response);
        total_busy += p[i].burst;
        sum_wait += p[i].waiting;
        sum_tat += p[i].turnaround;
        if (p[i].finish_time > last_finish)
            last_finish = p[i].finish_time;
    }

    printf("Avg waiting time    : %.2f\n", sum_wait / n);
    printf("Avg turnaround time : %.2f\n", sum_tat / n);
    printf("Throughput          : %.3f processes/unit\n", (double)n / last_finish);
    printf("CPU utilization     : %.1f%%\n", 100.0 * total_busy / last_finish);
}

int run_priority(Process p[], int n, int aging_interval)
{
    reset_processes(p, n);

    int last_ready[100]; // when each process last ran (or arrived), used for aging
    for (int i = 0; i < n; i++)
    {
        last_ready[i] = p[i].arrival;
        p[i].start_time = -1;
    }

    int time = 0;
    int completed = 0;
    int switches = 0;
    int last_run = -1;

    while (completed < n)
    {
        int pick = -1;
        int best = 0;
        for (int i = 0; i < n; i++)
        {
            if (p[i].done || p[i].arrival > time)
            {
                continue;
            }
            int effective = p[i].priority;
            if (aging_interval > 0)
            {
                effective -= (time - last_ready[i]) / aging_interval;
            }
            if (pick == -1 || effective < best)
            {
                pick = i;
                best = effective;
            }
        }

        if (pick == -1)
        {
            time++; // CPU idle for one tick
            continue;
        }

        if (p[pick].start_time == -1)
        {
            p[pick].start_time = time;
            p[pick].response = time - p[pick].arrival;
        }
        if (last_run != -1 && last_run != pick)
        {
            switches++;
        }

        p[pick].remaining--;
        time++;
        last_ready[pick] = time; // it just ran, so its waiting clock restarts
        last_run = pick;

        if (p[pick].remaining == 0)
        {
            p[pick].finish_time = time;
            p[pick].turnaround = p[pick].finish_time - p[pick].arrival;
            p[pick].waiting = p[pick].turnaround - p[pick].burst;
            p[pick].done = 1;
            completed++;
        }
    }

    return switches;
}

int run_mlfq(Process p[], int n, int boost_interval)
{
    int quantum[LEVELS] = {2, 4, 8};
    Queue queues[LEVELS];
    for (int l = 0; l < LEVELS; l++)
    {
        queues[l].head = 0;
        queues[l].tail = 0;
        queues[l].count = 0;
    }

    reset_processes(p, n);

    int level[100];
    int arrived[100];
    for (int i = 0; i < n; i++)
    {
        level[i] = 0;
        arrived[i] = 0;
        p[i].start_time = -1;
    }

    int time = 0;
    int completed = 0;
    int switches = 0;
    int last_run = -1;
    int running = -1; // index of the process on the CPU, -1 if none
    int slice_used = 0;

    while (completed < n)
    {
        // new arrivals always enter the top queue
        for (int i = 0; i < n; i++)
        {
            if (!arrived[i] && p[i].arrival <= time)
            {
                arrived[i] = 1;
                level[i] = 0;
                q_push(&queues[0], i);
            }
        }

        // periodic boost: everything goes back to the top queue
        if (boost_interval > 0 && time > 0 && time % boost_interval == 0)
        {
            if (running != -1)
            {
                level[running] = 0;
                q_push(&queues[0], running);
                running = -1;
                slice_used = 0;
            }
            for (int l = 1; l < LEVELS; l++)
            {
                while (queues[l].count > 0)
                {
                    int j = q_pop(&queues[l]);
                    level[j] = 0;
                    q_push(&queues[0], j);
                }
            }
        }

        // a process waiting in a higher queue preempts the running one
        if (running != -1)
        {
            for (int l = 0; l < level[running]; l++)
            {
                if (queues[l].count > 0)
                {
                    q_push(&queues[level[running]], running);
                    running = -1;
                    slice_used = 0;
                    break;
                }
            }
        }

        // if the CPU is free, take the head of the highest non-empty queue
        if (running == -1)
        {
            for (int l = 0; l < LEVELS; l++)
            {
                if (queues[l].count > 0)
                {
                    running = q_pop(&queues[l]);
                    slice_used = 0;
                    break;
                }
            }
        }

        if (running == -1)
        {
            time++; // CPU idle for one tick
            continue;
        }

        if (p[running].start_time == -1)
        {
            p[running].start_time = time;
            p[running].response = time - p[running].arrival;
        }
        if (last_run != -1 && last_run != running)
        {
            switches++;
        }

        // run for one tick
        p[running].remaining--;
        slice_used++;
        time++;
        last_run = running;

        if (p[running].remaining == 0)
        {
            p[running].finish_time = time;
            p[running].turnaround = p[running].finish_time - p[running].arrival;
            p[running].waiting = p[running].turnaround - p[running].burst;
            p[running].done = 1;
            completed++;
            running = -1;
        }
        else if (slice_used == quantum[level[running]])
        {
            // used its whole slice: demote one level (unless already at the bottom)
            if (level[running] < LEVELS - 1)
            {
                level[running]++;
            }
            q_push(&queues[level[running]], running);
            running = -1;
        }
    }

    return switches;
}

int main(void)
{
    Process procs[] = {
        {.pid = 1, .arrival = 0, .burst = 8, .priority = 2},
        {.pid = 2, .arrival = 1, .burst = 4, .priority = 1},
        {.pid = 3, .arrival = 2, .burst = 9, .priority = 3},
        {.pid = 4, .arrival = 3, .burst = 5, .priority = 2},
    };
    int n = sizeof(procs) / sizeof(procs[0]);

    run_fcfs(procs, n);
    print_report(procs, n, "FCFS");

    run_sjf(procs, n);
    print_report(procs, n, "SJF");

    int switches = run_rr(procs, n, 3);
    print_report(procs, n, "Round Robin (quantum 3)");
    printf("Context switches    : %d\n", switches);

    int pr_switches = run_priority(procs, n, 0);
    print_report(procs, n, "Priority (preemptive, no aging)");
    printf("Context switches    : %d\n", pr_switches);

    pr_switches = run_priority(procs, n, 5);
    print_report(procs, n, "Priority (preemptive, aging every 5)");
    printf("Context switches    : %d\n", pr_switches);

    int mlfq_switches = run_mlfq(procs, n, 0);
    print_report(procs, n, "MLFQ (quanta 2/4/8, no boost)");
    printf("Context switches    : %d\n", mlfq_switches);

    mlfq_switches = run_mlfq(procs, n, 10);
    print_report(procs, n, "MLFQ (quanta 2/4/8, boost every 10)");
    printf("Context switches    : %d\n", mlfq_switches);

    return 0;
}