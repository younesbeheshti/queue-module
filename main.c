#include <arpa/inet.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>

#include "shared_memory.h"

#define QUEUE_DEVICE "/dev/myQueue"
#define SHARED_BUFFER_SIZE 1024

int shared_mem_id;

// Function declarations
void serverProcess();
void clientProcess();
int queueHandler();
void writeToDevice(char *data);
double getCurrentEpoch();
long int getEpochMillis();
void signalHandler(int signal);

int activeFlag = 1;
char bufferData[SHARED_BUFFER_SIZE];

int main() {
    pid_t parent_pid = getpid();
    long int *timestamps;
    int shm_key = ftok(".", 34);
    int shm_time_id = shmget(shm_key, sizeof(long int) * 6, 0666 | IPC_CREAT);

    fork();
    if (getpid() != parent_pid) {
        // First child process
        serverProcess();
        timestamps = (long int *)shmat(shm_time_id, NULL, 0);
        timestamps[0] = getpid();
        timestamps[1] = getEpochMillis();
        shmdt((void *)timestamps);
        exit(0);
    }

    fork();
    if (getpid() != parent_pid) {
        // Second child process
        clientProcess();
        timestamps = (long int *)shmat(shm_time_id, NULL, 0);
        timestamps[2] = getpid();
        timestamps[3] = getEpochMillis();
        shmdt((void *)timestamps);
        exit(0);
    }

    int status;
    timestamps = (long int *)shmat(shm_time_id, NULL, 0);

    waitpid(timestamps[0], &status, 0);
    printf("Server process wait time: %ld ms\n", getEpochMillis() - timestamps[1]);

    shared_mem_id = initializeSharedMemory();
    fork();
    if (getpid() != parent_pid) {
        // Third child process
        int queueSuccess = queueHandler();
        timestamps = (long int *)shmat(shm_time_id, NULL, 0);
        timestamps[4] = getpid();
        timestamps[5] = getEpochMillis();
        shmdt((void *)timestamps);
        if (queueSuccess == 0) {
            printf("Queue handler completed successfully.\n");
            exit(0);
        } else {
            perror("Queue handler failed!\n");
            exit(1);
        }
    }

    for (int i = 0; i < 2; ++i) {
        int completed_pid = wait(NULL);
        if (completed_pid == timestamps[2]) {
            printf("Client process wait time: %ld ms\n", getEpochMillis() - timestamps[3]);
        } else {
            printf("Queue handler wait time: %ld ms\n", getEpochMillis() - timestamps[5]);
        }
    }

    char readData[SHARED_BUFFER_SIZE];
    fetchDataFromMemory(readData, shared_mem_id);
    printf("Data from shared memory: \"%s\"\n", readData);

    cleanUpSharedMemory(shared_mem_id);
    shmctl(shm_time_id, IPC_RMID, NULL);
    return 0;
}

double getCurrentEpoch() {
    struct timespec timeSpec;
    clock_gettime(CLOCK_REALTIME, &timeSpec);
    return timeSpec.tv_sec + timeSpec.tv_nsec * 1e-9;
}

long int getEpochMillis() {
    return (long int)(round(getCurrentEpoch() * 1000));
}

void serverProcess() {
    char inputData[1024];
    int socket_fd, new_client_fd;
    struct sockaddr_in server_addr;

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(54321);

    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Socket bind failed.");
        exit(EXIT_FAILURE);
    }

    if (listen(socket_fd, 3) < 0) {
        perror("Listening on socket failed.");
        exit(EXIT_FAILURE);
    }

    printf("Enter message to send: ");
    fgets(inputData, sizeof(inputData), stdin);

    socklen_t addr_len = sizeof(server_addr);
    new_client_fd = accept(socket_fd, (struct sockaddr *)&server_addr, &addr_len);
    send(new_client_fd, inputData, strlen(inputData), 0);
}

void clientProcess() {
    int socket_fd;
    struct sockaddr_in server_addr;

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(54321);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        printf("Invalid address or unsupported address.\n");
        exit(EXIT_FAILURE);
    }

    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Connection to server failed.\n");
        exit(EXIT_FAILURE);
    }

    char response[SHARED_BUFFER_SIZE] = {0};
    read(socket_fd, response, SHARED_BUFFER_SIZE);
    printf("Received from server: %s\n", response);
    writeToDevice(response);
}

void signalHandler(int signal) {
    printf("Captured data: %s\n", bufferData);
    storeDataToMemory(shared_mem_id, bufferData);
    activeFlag = 0;
}

int queueHandler() {
    signal(SIGINT, signalHandler);
    int queue_fd = open(QUEUE_DEVICE, O_RDWR);
    int readSuccess;

    int index = 0;
    while (activeFlag) {
        readSuccess = read(queue_fd, bufferData + index, 1);
        if (readSuccess < 1) break;
        index++;
    }

    printf("Queue read: %s\n", bufferData);
    storeDataToMemory(shared_mem_id, bufferData);
    return 0;
}

void writeToDevice(char *data) {
    int device_fd = open(QUEUE_DEVICE, O_RDWR);
    for (size_t i = 0; i < strlen(data); i++) {
        if (write(device_fd, &data[i], 1) < 0) {
            printf("Write failed.\n");
            break;
        }
    }
}
