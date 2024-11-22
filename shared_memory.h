#define BUF_SIZE 1024
int create_shm();
int remove_shm(int shmid);
int write_data(int shmid, char *data);
int read_data(char *data, int shmid);
