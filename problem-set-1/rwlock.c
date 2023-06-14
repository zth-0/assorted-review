#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define READER_THREAD_NUM 20
#define WRITER_THREAD_NUM 20

// global variable protected by the reader writer lock
int globalVar;

// reader conditional variable
pthread_cond_t rcv;

// writer conditional variable
pthread_cond_t wcv;

// mutex for the reader writer lock
pthread_mutex_t mux;

// resource counter, with values for the following scenarios
//   counter = -1;         One writer thread has the rwlock
//   counter = 0;          No reader nor writer has the rwlock
//   counter = N; (N > 0)  N readers has have the rwlock
int counter;

typedef struct RWLock {
    pthread_cond_t* reader_cv;
    pthread_cond_t* writer_cv;
    pthread_mutex_t* mux;
    int counter;
} RWLock;

RWLock rwlock;

void RLock(RWLock* rwl) {
    pthread_mutex_lock(rwl->mux);
    // wait until writer thread release lock
    while (rwl->counter == -1) {
        pthread_cond_wait(rwl->reader_cv, rwl->mux);
    }
    rwl->counter++;
    pthread_mutex_unlock(rwl->mux);
}

void RUnlock(RWLock* rwl) {
    pthread_mutex_lock(rwl->mux);
    rwl->counter--;
    if (rwl->counter == 0) {
        pthread_cond_signal(rwl->writer_cv);
    }
    pthread_mutex_unlock(rwl->mux);
}

void Lock(RWLock* rwl) {
    pthread_mutex_lock(rwl->mux);
    while (rwl->counter != 0) {
        pthread_cond_wait(rwl->writer_cv, rwl->mux);
    }
    rwl->counter = -1;
    pthread_mutex_unlock(rwl->mux);
}

void Unlock(RWLock* rwl) {
    pthread_mutex_lock(rwl->mux);
    rwl->counter = 0;
    pthread_cond_broadcast(rwl->reader_cv);
    pthread_cond_signal(rwl->writer_cv);
    pthread_mutex_unlock(rwl->mux);
}

// initialize variables
int init() {
    int ret;
    ret = pthread_cond_init(&rcv, NULL);
    if (ret != 0) {
        perror("pthread_cond_init failed for rcv\n");
        return ret;
    }

    ret = pthread_cond_init(&wcv, NULL);
    if (ret != 0) {
        perror("pthread_cond_init failed for wcv\n");
        return ret;
    }

    ret = pthread_mutex_init(&mux, NULL);
    if (ret != 0) {
        perror("pthread_mutex_init failed for mux\n");
        return ret;
    }

    rwlock.counter = 0;
    rwlock.mux = &mux;
    rwlock.reader_cv = &rcv;
    rwlock.writer_cv = &wcv;

    return ret;
}

void cleanUp() {
    if (pthread_cond_destroy(&rcv) != 0) {
        perror("failed to destroy rcv.\n");
    }
    
    if (pthread_cond_destroy(&wcv) != 0) {
        perror("failed to destroy wcv.\n");
    }

    if (pthread_mutex_destroy(&mux) != 0) {
        perror("failed to destroy mux.\n");
    }
}

void *reader(void *pArg) {
    int* arg = (int*)pArg;
    RLock(&rwlock);

    // add some randomness
    usleep(rand() % 53);
    printf("globalVar: %d; counter: %d; reader: %d\n", 
            globalVar, 
            rwlock.counter,
            *arg);
    RUnlock(&rwlock);
}

void *writer(void *pArg) {
    int* arg = (int*)pArg;
    Lock(&rwlock);

    // add some randomness
    usleep(rand() % 61);
    globalVar = *arg;
    printf("globalVar: %d; counter: %d; writer: %d\n",
            globalVar,
            rwlock.counter,
            *arg - 100);
    Unlock(&rwlock);
}

int main() {
    printf("init...\n");
    int ret = init();
    if (ret != 0) {
        exit(1);
    }

    printf("starting readers and writers...\n");
    
    pthread_t rths[READER_THREAD_NUM];
    int rvals[READER_THREAD_NUM];

    pthread_t wths[WRITER_THREAD_NUM];
    int wvals[WRITER_THREAD_NUM];

    for (int i = 0; i < READER_THREAD_NUM; i++) {
        rvals[i] = i;
        pthread_create(&rths[i], NULL, reader, &rvals[i]);
        wvals[i] = 100 + i;
        pthread_create(&wths[i], NULL, writer, &wvals[i]);
    }

    printf("joining readers...\n");
    for (int i = 0; i < READER_THREAD_NUM; i++) {
        pthread_join(rths[i], NULL);

    }

    printf("joining writers...\n");
    for (int i = 0; i < WRITER_THREAD_NUM; i++) {
        pthread_join(wths[i], NULL);
    }

    cleanUp();
    exit(0);
}