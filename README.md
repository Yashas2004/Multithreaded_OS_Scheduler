# Multithreaded OS Scheduler in C

A scheduling simulator and a userspace multithreaded task scheduler, built from scratch in C with POSIX threads. It covers classic scheduling algorithms, thread synchronization, deadlock detection, starvation prevention and work stealing, with measurements at every stage.

## What's inside

| File                          | What it does                                                                                                                       |
| ----------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| `src/main.c`, `src/process.h` | Discrete-time simulator: FCFS, SJF, Round Robin, preemptive Priority (with aging), Multilevel Feedback Queue (with periodic boost) |
| `src/queue_demo.c`            | Demonstrates a race condition on a shared counter and the mutex fix                                                                |
| `src/worker_pool.c`           | Worker threads pulling from a mutex-protected queue                                                                                |
| `src/worker_pool_sem.c`       | Bounded queue using counting semaphores, with tasks arriving over time                                                             |
| `src/deadlock_demo.c`         | Real deadlock, plus wait-for graph cycle detection with victim backoff                                                             |
| `src/work_stealing.c`         | Per-worker queues with work stealing                                                                                               |
| `src/bench.c`                 | Context-switch overhead and scheduling latency benchmark                                                                           |

## Build

Requires gcc with pthread support (MSYS2 UCRT64 on Windows, or any Linux/macOS gcc).

```
gcc -Wall -Wextra -pthread -o scheduler src/main.c
gcc -Wall -Wextra -pthread -DNUM_WORKERS=3 -o pool3 src/worker_pool.c
gcc -Wall -Wextra -pthread -DSTEAL -o steal_on src/work_stealing.c
```

## Results

### Scheduling algorithms (simulator)

Four processes with arrivals 0, 1, 2, 3 and bursts 8, 4, 9, 5.

| Algorithm                    | Avg wait | Avg turnaround | Context switches |
| ---------------------------- | -------- | -------------- | ---------------- |
| FCFS                         | 8.75     | 15.25          | n/a              |
| SJF                          | 7.75     | 14.25          | n/a              |
| Round Robin (quantum 3)      | 13.50    | 20.00          | 9                |
| Priority, no aging           | 7.00     | 13.50          | 4                |
| Priority, aging every 5      | 7.75     | 14.25          | 8                |
| MLFQ (2/4/8), no boost       | 13.00    | 19.50          | 9                |
| MLFQ (2/4/8), boost every 10 | 12.75    | 19.25          | 11               |

Round Robin and MLFQ have worse averages but far better response times (first-run delay of 0 to 6 versus 0 to 18 for FCFS). Aging and boosting trade a little average performance for bounded waiting, which prevents starvation.

### Worker pool scaling (mutex-protected queue)

12 tasks, all ready at time 0.

| Workers | Elapsed (ms) | Avg wait (ms) | Throughput (tasks/s) | Utilization |
| ------- | ------------ | ------------- | -------------------- | ----------- |
| 1       | 2740         | 1282.7        | 4.38                 | 96.7%       |
| 3       | 1008         | 331.0         | 11.91                | 87.7%       |
| 6       | 562          | 105.0         | 21.34                | 78.5%       |

### Semaphore-bounded queue (tasks arrive every 80 ms)

| Workers | Elapsed (ms) | Avg wait (ms) | Throughput (tasks/s) | Utilization |
| ------- | ------------ | ------------- | -------------------- | ----------- |
| 1       | 2819         | 641.2         | 4.26                 | 94.0%       |
| 3       | 1299         | 6.6           | 9.24                 | 68.0%       |
| 6       | 1285         | 0.0           | 9.34                 | 34.4%       |

With a limited arrival rate, extra workers stop helping: three workers already keep up.

### Work stealing (deliberately unbalanced load)

Worker 0 starts with 10 tasks, workers 1 and 2 with 3 each.

|              | Elapsed (ms) | Avg wait (ms) | Avg turnaround (ms) | Throughput (tasks/s) | Utilization |
| ------------ | ------------ | ------------- | ------------------- | -------------------- | ----------- |
| Stealing off | 2774         | 888.1         | 1108.0              | 5.77                 | 42.3%       |
| Stealing on  | 1299         | 391.9         | 611.8               | 12.31                | 90.2%       |

### Race condition demo

1,000,000 tasks handed out across 4 threads. Without a mutex, 3.4 to 3.7 million were claimed (tasks run multiple times). With the mutex, exactly 1,000,000.

### Context switches and latency

| Metric                                   | Idle  | With 16 competing threads |
| ---------------------------------------- | ----- | ------------------------- |
| Context-switch overhead (us/switch)      | 3.3   | 3.2                       |
| Scheduling latency, median (us)          | 4.8   | 4.6                       |
| Scheduling latency, 99th percentile (us) | 22.4  | 8.3                       |
| Scheduling latency, worst (us)           | 120.9 | 70,659                    |

Under load the median and 99th percentile stayed low (Windows boosts the priority of threads waking from a wait), but a single rare stall of about 70 ms dominated the average. Averages hide this, so percentiles are reported instead. |

## Design notes

- The mutex guards only the short section that claims a task, never the task's execution.
- Semaphores handle blocking (workers sleep on an empty queue, the producer blocks on a full one); the mutex only protects the queue indexes.
- Deadlock detection follows the wait-for chain before a thread blocks. A cycle means the requester is the victim: it releases its locks, backs off and retries.
- Thieves take from the back of a victim's queue while owners take from the front, and no thread ever holds two queue locks, so stealing cannot deadlock.

## Limitations

- Task execution is simulated with sleeps, so the numbers show scheduling behavior rather than real CPU contention. Windows `Sleep` overshoots by 5 to 15 ms per call.
- Each configuration was run once; timings vary slightly between runs.
- Work stealing here takes one task from the back; stealing half a queue or the largest task would reduce the tail effect seen in the results.
- Switch overhead includes cross-core wake-up cost because threads are not pinned.
