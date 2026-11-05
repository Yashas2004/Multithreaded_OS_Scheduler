#include <stdio.h>
#include <pthread.h>

#define NUM_TASKS 1000000
#define NUM_WORKERS 4

#ifdef USE_MUTEX
#define LOCK() pthread_mutex_lock(&lock)
#define UNLOCK() pthread_mutex_unlock(&lock)
#else
#define LOCK()
#define UNLOCK()
#endif

int next_task = 0;         // shared: the next task number to hand out
int taken_by[NUM_WORKERS]; // how many tasks each worker grabbed
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *worker(void *arg)
{
    int me = *(int *)arg;

    while (1)
    {
        LOCK();
        int id = next_task; // read the counter...
        if (id >= NUM_TASKS)
        {
            UNLOCK();
            break;
        }
        next_task = id + 1; // ...then write it back
        UNLOCK();

        taken_by[me]++; // only this worker touches its own slot
    }
    return NULL;
}

int main(void)
{
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

    int total = 0;
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        printf("Worker %d took %d tasks\n", i, taken_by[i]);
        total += taken_by[i];
    }
    printf("Total taken: %d (expected %d)\n", total, NUM_TASKS);
    return 0;
}