#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define SWITCH_ROUNDS 100000
#define LATENCY_SAMPLES 5000
#ifndef LOAD
#define LOAD 0 // number of busy-spinning threads competing for the CPU
#endif

double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
}

/* ---------- Part 1: context-switch overhead ---------- */

sem_t ping, pong;

void *ponger(void *arg)
{
    (void)arg;
    for (int i = 0; i < SWITCH_ROUNDS; i++)
    {
        sem_wait(&ping);
        sem_post(&pong);
    }
    return NULL;
}

double measure_switch_cost(void)
{
    sem_init(&ping, 0, 0);
    sem_init(&pong, 0, 0);

    // baseline: the same semaphore calls with no thread switch involved
    sem_t solo;
    sem_init(&solo, 0, 0);
    double start = now_us();
    for (int i = 0; i < SWITCH_ROUNDS; i++)
    {
        sem_post(&solo);
        sem_wait(&solo);
    }
    double baseline = (now_us() - start) / SWITCH_ROUNDS; // one post + one wait

    // ping-pong: every round trip forces two thread switches
    pthread_t t;
    pthread_create(&t, NULL, ponger, NULL);
    start = now_us();
    for (int i = 0; i < SWITCH_ROUNDS; i++)
    {
        sem_post(&ping);
        sem_wait(&pong);
    }
    double roundtrip = (now_us() - start) / SWITCH_ROUNDS;
    pthread_join(t, NULL);

    double per_switch = (roundtrip - 2 * baseline) / 2;
    printf("Semaphore call cost (no switch) : %.3f us per post+wait\n", baseline);
    printf("Ping-pong round trip            : %.3f us\n", roundtrip);
    printf("Context-switch overhead         : %.3f us per switch\n", per_switch);

    sem_destroy(&ping);
    sem_destroy(&pong);
    sem_destroy(&solo);
    return per_switch;
}

/* ---------- Part 2: scheduling latency ---------- */

sem_t wake, ack;
double post_time = 0; // written before sem_post, read after sem_wait, so it is safe
double latencies[LATENCY_SAMPLES];
volatile int stop_spinners = 0;

void *sleeper(void *arg)
{
    (void)arg;
    for (int i = 0; i < LATENCY_SAMPLES; i++)
    {
        sem_wait(&wake);
        latencies[i] = now_us() - post_time; // time from "ready" to "running"
        sem_post(&ack);
    }
    return NULL;
}

void *spinner(void *arg)
{
    (void)arg;
    while (!stop_spinners)
    {
        // burn CPU so the sleeper has to compete for a core
    }
    return NULL;
}

void sort_latencies(void)
{
    for (int i = 1; i < LATENCY_SAMPLES; i++)
    {
        double key = latencies[i];
        int j = i - 1;
        while (j >= 0 && latencies[j] > key)
        {
            latencies[j + 1] = latencies[j];
            j--;
        }
        latencies[j + 1] = key;
    }
}

void measure_latency(void)
{
    sem_init(&wake, 0, 0);
    sem_init(&ack, 0, 0);

    pthread_t spinners[64];
    for (int i = 0; i < LOAD; i++)
    {
        pthread_create(&spinners[i], NULL, spinner, NULL);
    }

    pthread_t t;
    pthread_create(&t, NULL, sleeper, NULL);

    for (int i = 0; i < LATENCY_SAMPLES; i++)
    {
        // spin ~100 us so the sleeper is really blocked again before we wake it
        double until = now_us() + 100.0;
        while (now_us() < until)
        {
        }
        post_time = now_us();
        sem_post(&wake);
        sem_wait(&ack);
    }
    pthread_join(t, NULL);

    stop_spinners = 1;
    for (int i = 0; i < LOAD; i++)
    {
        pthread_join(spinners[i], NULL);
    }

    sort_latencies();
    double sum = 0;
    for (int i = 0; i < LATENCY_SAMPLES; i++)
    {
        sum += latencies[i];
    }

    printf("Competing busy threads          : %d\n", LOAD);
    printf("Scheduling latency average      : %.2f us\n", sum / LATENCY_SAMPLES);
    printf("Scheduling latency median       : %.2f us\n", latencies[LATENCY_SAMPLES / 2]);
    printf("Scheduling latency 99th pct     : %.2f us\n", latencies[(LATENCY_SAMPLES * 99) / 100]);
    printf("Scheduling latency worst        : %.2f us\n", latencies[LATENCY_SAMPLES - 1]);

    sem_destroy(&wake);
    sem_destroy(&ack);
}

int main(void)
{
    printf("=== Context-switch overhead (%d round trips) ===\n", SWITCH_ROUNDS);
    measure_switch_cost();

    printf("\n=== Scheduling latency (%d samples) ===\n", LATENCY_SAMPLES);
    measure_latency();
    return 0;
}