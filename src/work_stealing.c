#include <stdio.h>
#include <pthread.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define NUM_TASKS 16
#define NUM_WORKERS 3

typedef struct
{
    int id;
    int burst_ms;
    double start_time;
    double finish_time;
    int worker;
    int stolen;
} Task;

typedef struct
{
    int items[NUM_TASKS];
    int head, tail; // tasks live in items[head .. tail-1]
    pthread_mutex_t lock;
} Deque;

Task tasks[NUM_TASKS];
Deque deques[NUM_WORKERS];
int steals[NUM_WORKERS]; // each worker writes only its own slot
int tasks_run[NUM_WORKERS];
double busy_ms[NUM_WORKERS];
double base_ms = 0;

double raw_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

double now_ms(void)
{
    return raw_ms() - base_ms;
}

void sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

// owner takes from the front of its own queue
int take_own(Deque *d)
{
    int idx = -1;
    pthread_mutex_lock(&d->lock);
    if (d->head < d->tail)
    {
        idx = d->items[d->head];
        d->head++;
    }
    pthread_mutex_unlock(&d->lock);
    return idx;
}

// thieves take from the back, away from where the owner is working
int steal_from(Deque *d)
{
    int idx = -1;
    pthread_mutex_lock(&d->lock);
    if (d->head < d->tail)
    {
        d->tail--;
        idx = d->items[d->tail];
    }
    pthread_mutex_unlock(&d->lock);
    return idx;
}

void *worker(void *arg)
{
    int me = *(int *)arg;

    while (1)
    {
        int idx = take_own(&deques[me]);
        int stolen = 0;

        if (idx == -1)
        {
#ifdef STEAL
            // own queue is empty: try each other worker's queue in turn
            for (int k = 1; k < NUM_WORKERS && idx == -1; k++)
            {
                int victim = (me + k) % NUM_WORKERS;
                idx = steal_from(&deques[victim]);
            }
            if (idx != -1)
            {
                stolen = 1;
                steals[me]++;
            }
#endif
        }

        if (idx == -1)
        {
            break; // nothing left anywhere we are allowed to look
        }

        Task *t = &tasks[idx];
        t->worker = me;
        t->stolen = stolen;
        t->start_time = now_ms();
        sleep_ms(t->burst_ms); // stands in for real CPU work
        t->finish_time = now_ms();

        busy_ms[me] += t->finish_time - t->start_time;
        tasks_run[me]++;
    }
    return NULL;
}

int main(void)
{
    int bursts[NUM_TASKS] = {300, 250, 400, 200, 350, 300, 250, 200, 300, 150,
                             100, 150, 100,
                             120, 100, 130};

    for (int w = 0; w < NUM_WORKERS; w++)
    {
        deques[w].head = 0;
        deques[w].tail = 0;
        pthread_mutex_init(&deques[w].lock, NULL);
    }

    // deliberately unfair split: worker 0 gets tasks 1-10, worker 1 gets 11-13, worker 2 gets 14-16
    for (int i = 0; i < NUM_TASKS; i++)
    {
        tasks[i].id = i + 1;
        tasks[i].burst_ms = bursts[i];

        int owner = 2;
        if (i < 10)
        {
            owner = 0;
        }
        else if (i < 13)
        {
            owner = 1;
        }
        deques[owner].items[deques[owner].tail] = i;
        deques[owner].tail++;
    }

    base_ms = raw_ms();

    pthread_t threads[NUM_WORKERS];
    int ids[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        ids[i] = i;
        pthread_create(&threads[i], NULL, worker, &ids[i]);
    }
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        pthread_join(threads[i], NULL);
    }
    double elapsed = now_ms();

#ifdef STEAL
    printf("Work stealing: ON\n");
#else
    printf("Work stealing: OFF\n");
#endif

    double sum_wait = 0, sum_tat = 0, total_busy = 0;
    printf("Task Worker Stolen Burst Wait(ms) Turnaround(ms)\n");
    for (int i = 0; i < NUM_TASKS; i++)
    {
        double wait = tasks[i].start_time; // every task is ready at time 0
        double tat = tasks[i].finish_time;
        printf("%-4d %-6d %-6s %-5d %-9.0f %-14.0f\n",
               tasks[i].id, tasks[i].worker, tasks[i].stolen ? "yes" : "-",
               tasks[i].burst_ms, wait, tat);
        sum_wait += wait;
        sum_tat += tat;
    }

    printf("\nPer-worker results\n");
    for (int w = 0; w < NUM_WORKERS; w++)
    {
        printf("Worker %d: ran %d tasks, stole %d, busy %.0f ms (%.1f%% of run)\n",
               w, tasks_run[w], steals[w], busy_ms[w], 100.0 * busy_ms[w] / elapsed);
        total_busy += busy_ms[w];
    }

    printf("\nAvg waiting time    : %.1f ms\n", sum_wait / NUM_TASKS);
    printf("Avg turnaround time : %.1f ms\n", sum_tat / NUM_TASKS);
    printf("Total elapsed       : %.0f ms\n", elapsed);
    printf("Throughput          : %.2f tasks/sec\n", NUM_TASKS / (elapsed / 1000.0));
    printf("Worker utilization  : %.1f%%\n", 100.0 * total_busy / (NUM_WORKERS * elapsed));
    return 0;
}