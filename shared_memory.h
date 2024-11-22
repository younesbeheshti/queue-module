#define SHARED_BUFFER_SIZE 1024

int initializeSharedMemory();
int cleanUpSharedMemory(int shmid);
int storeDataToMemory(int shmid, char *data);
int fetchDataFromMemory(char *data, int shmid);
