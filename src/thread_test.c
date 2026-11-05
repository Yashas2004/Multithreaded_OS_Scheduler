#include <stdio.h>
#include <pthread.h>

void *hello(void *arg)
{
    int id = *(int *)arg;
    printf("Hello from worker thread %d\n", id);
    return NULL;
}

int main(void)
{
    pthread_t threads[3];
    int ids[3] = {0, 1, 2};

    for (int i = 0; i < 3; i++)
    {
        pthread_create(&threads[i], NULL, hello, &ids[i]);
    }
    for (int i = 0; i < 3; i++)
    {
        pthread_join(threads[i], NULL);
    }
    printf("All workers finished\n");
    return 0;
}