#include <stdio.h>
#include <pthread.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define NUM_TASKS 12
#ifndef NUM_WORKERS
#define NUM_WORKERS 3
#endif

typedef struct
{
    int id;
    int burst_ms;
    double submit_time; // ms since program start
    double start_time;
    double finish_time;
    int worker;
} Task;

Task tasks[NUM_TASKS];
int next_task = 0; // shared queue position, protected by queue_lock
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
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

void *worker(void *arg)
{
    int me = *(int *)arg;

    while (1)
    {
        pthread_mutex_lock(&queue_lock);
        if (next_task >= NUM_TASKS)
        {
            pthread_mutex_unlock(&queue_lock);
            break;
        }
        int idx = next_task;
        next_task++;
        pthread_mutex_unlock(&queue_lock);

        // each task is owned by exactly one worker from here on
        Task *t = &tasks[idx];
        t->worker = me;
        t->start_time = now_ms();
        sleep_ms(t->burst_ms); // stands in for real CPU work
        t->finish_time = now_ms();
    }
    return NULL;
}

int main(void)
{
    int bursts[NUM_TASKS] = {300, 100, 200, 400, 150, 250, 100, 350, 200, 120, 300, 180};

    base_ms = raw_ms();
    for (int i = 0; i < NUM_TASKS; i++)
    {
        tasks[i].id = i + 1;
        tasks[i].burst_ms = bursts[i];
        tasks[i].submit_time = now_ms();
    }

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

    double sum_wait = 0, sum_tat = 0, total_burst = 0;
    printf("Workers: %d\n", NUM_WORKERS);
    printf("Task Worker Burst Wait(ms) Turnaround(ms)\n");
    for (int i = 0; i < NUM_TASKS; i++)
    {
        double wait = tasks[i].start_time - tasks[i].submit_time;
        double tat = tasks[i].finish_time - tasks[i].submit_time;
        printf("%-4d %-6d %-5d %-9.0f %-14.0f\n",
               tasks[i].id, tasks[i].worker, tasks[i].burst_ms, wait, tat);
        sum_wait += wait;
        sum_tat += tat;
        total_burst += tasks[i].burst_ms;
    }

    printf("Avg waiting time    : %.1f ms\n", sum_wait / NUM_TASKS);
    printf("Avg turnaround time : %.1f ms\n", sum_tat / NUM_TASKS);
    printf("Total elapsed       : %.0f ms\n", elapsed);
    printf("Throughput          : %.2f tasks/sec\n", NUM_TASKS / (elapsed / 1000.0));
    printf("Worker utilization  : %.1f%%\n", 100.0 * total_burst / (NUM_WORKERS * elapsed));
    return 0;
}