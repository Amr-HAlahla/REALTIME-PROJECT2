#include "local.h"

int openSHM(char *name, int size);
void *mapSHM(int shmid, int size);
void closeSHM(void *shm, int size);
sem_t *openSEM(char *name, int value);
void lockSEM(sem_t *sem);
void unlockSEM(sem_t *sem);

sem_t *openSEM(char *name, int value)
{
    sem_t *semid = sem_open(name, O_CREAT | O_RDWR, 0666, value);
    if (semid == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }
    return semid;
}

void lockSEM(sem_t *sem)
{
    if (sem_wait(&sem) == -1)
    {
        perror("sem_wait");
        exit(1);
    }
}

void unlockSEM(sem_t *sem)
{
    if (sem_post(&sem) == -1)
    {
        perror("sem_post");
        exit(1);
    }
}

int openSHM(char *name, int size)
{
    int shmid = shm_open(name, O_CREAT | O_RDWR, 0666);
    // try to craete the shared memory, if it already exists, open it
    if (shmid == -1)
    {
        perror("shm_open");
        exit(1);
    }
    if (ftruncate(shmid, size) == -1)
    {
        perror("ftruncate");
        exit(1);
    }
    return shmid;
}

void *mapSHM(int shmid, int size)
{
    void *shm;
    if ((shm = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0)) == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }
    return shm;
}

void closeSHM(void *shm, int size)
{
    if (munmap(shm, size) == -1)
    {
        perror("munmap");
        exit(1);
    }
}