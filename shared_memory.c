#include <sys/shm.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "shared_memory.h"

#define SHM_KEY 0x2345

typedef struct {
    int dataSize;
    int isReady;
    char buffer[SHARED_BUFFER_SIZE];
} SharedMemory;

int initializeSharedMemory() {
    int shmid = shmget(SHM_KEY, sizeof(SharedMemory), 0644 | IPC_CREAT);
    if (shmid == -1) {
        perror("Failed to create shared memory");
        return -1;
    }
    return shmid;
}

int cleanUpSharedMemory(int shmid) {
    return shmctl(shmid, IPC_RMID, NULL);
}

int storeDataToMemory(int shmid, char *data) {
    SharedMemory *sharedMem = shmat(shmid, NULL, 0);
    if (sharedMem == (void *)-1) {
        perror("Shared memory attachment failed");
        return -1;
    }
    strcpy(sharedMem->buffer, data);
    sharedMem->dataSize = strlen(data);
    sharedMem->isReady = 1;
    shmdt(sharedMem);
    return 0;
}

int fetchDataFromMemory(char *data, int shmid) {
    SharedMemory *sharedMem = shmat(shmid, NULL, 0);
    if (sharedMem == (void *)-1) {
        perror("Shared memory attachment failed");
        return -1;
    }
    if (sharedMem->isReady) {
        strcpy(data, sharedMem->buffer);
        shmdt(sharedMem);
        return 0;
    }
    shmdt(sharedMem);
    return -1;
}
