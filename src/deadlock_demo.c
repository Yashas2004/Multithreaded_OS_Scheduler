#include <stdio.h>
#include <pthread.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define NUM_THREADS 2
#define NUM_LOCKS 2

void sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

#ifdef DETECT

pthread_mutex_t graph_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t released = PTHREAD_COND_INITIALIZER;
int owner[NUM_LOCKS] = {-1, -1};         // which thread holds each lock (-1 = free)
int waiting_for[NUM_THREADS] = {-1, -1}; // which lock each thread is blocked on

// Follow the chain of "waits for lock -> held by thread -> waits for lock..."
// A chain that comes back to 'me' is a cycle, which means deadlock.
int has_cycle(int me)
{
    int current = me;
    for (int step = 0; step < NUM_THREADS; step++)
    {
        int lock = waiting_for[current];
        if (lock == -1)
        {
            return 0; // this thread isn't waiting, so the chain ends
        }
        int holder = owner[lock];
        if (holder == -1)
        {
            return 0; // lock is free, no problem
        }
        if (holder == me)
        {
            return 1; // chain led back to me: cycle
        }
        current = holder;
    }
    return 0;
}

// returns 0 when the lock is acquired, -1 if waiting would deadlock
int acquire(int me, int lock)
{
    pthread_mutex_lock(&graph_lock);
    while (owner[lock] != -1)
    {
        waiting_for[me] = lock;
        if (has_cycle(me))
        {
            waiting_for[me] = -1;
            pthread_mutex_unlock(&graph_lock);
            return -1;
        }
        pthread_cond_wait(&released, &graph_lock); // sleep until some lock is released
    }
    waiting_for[me] = -1;
    owner[lock] = me;
    pthread_mutex_unlock(&graph_lock);
    return 0;
}

void release(int me, int lock)
{
    (void)me;
    pthread_mutex_lock(&graph_lock);
    owner[lock] = -1;
    pthread_cond_broadcast(&released);
    pthread_mutex_unlock(&graph_lock);
}

#else

// plain mutexes: no detection, so this version really deadlocks
pthread_mutex_t locks[NUM_LOCKS] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};

int acquire(int me, int lock)
{
    (void)me;
    pthread_mutex_lock(&locks[lock]);
    return 0;
}

void release(int me, int lock)
{
    (void)me;
    pthread_mutex_unlock(&locks[lock]);
}

#endif

typedef struct
{
    int id;
    int first;  // lock this thread takes first
    int second; // lock it takes second
} Job;

void *worker(void *arg)
{
    Job *job = (Job *)arg;
    int me = job->id;
    const char names[] = "AB";

    while (1)
    {
        acquire(me, job->first);
        printf("Thread %d holds lock %c\n", me, names[job->first]);
        sleep_ms(100); // long enough for the other thread to grab its first lock

        printf("Thread %d wants lock %c\n", me, names[job->second]);
        if (acquire(me, job->second) == -1)
        {
            printf("Thread %d: DEADLOCK detected, releasing lock %c and backing off\n",
                   me, names[job->first]);
            release(me, job->first);
            sleep_ms(100 + 50 * me); // back off so the other thread can finish
            continue;
        }

        printf("Thread %d holds both locks, working\n", me);
        sleep_ms(50);
        release(me, job->second);
        release(me, job->first);
        printf("Thread %d done\n", me);
        return NULL;
    }
}

int main(void)
{
    // opposite lock order: this is the classic deadlock recipe
    Job jobs[NUM_THREADS] = {{0, 0, 1}, {1, 1, 0}};
    pthread_t threads[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_create(&threads[i], NULL, worker, &jobs[i]);
    }
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }
    printf("Both threads finished\n");
    return 0;
}