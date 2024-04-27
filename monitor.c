#include "functions.c"
#include "local.h"

void setupSignals();
void signalHandler(int sig);
void updateHeights();
void open_shm_sem();

int cont_shmid;
int *cont_shm_ptr;
int data_shmid;
int *data_shm_ptr;
sem_t *sem_containers;
sem_t *sem_data;

int totalContainersDropped = 0;

int period = 10; // Period for updating heights

int main(int argc, char *argv[])
{
    setupSignals();
    open_shm_sem();
    while (1)
    {
        // sleep(5);
        // alarm(period);
        pause();
    }
}

void updateHeights()
{
    // printf("MONITOR Process Enetered updateHeights\n");
    if (sem_wait(sem_data) == -1)
    {
        perror("waitSEM");
        exit(1);
    }
    int totalContainersDropped = ((SharedData *)data_shm_ptr)->totalContainersDropped;
    if (sem_wait(sem_containers) == -1)
    {
        perror("waitSEM");
        exit(1);
    }
    printf("\033[0;32mStart Monitoring...\n\033[0m");
    int *temp = cont_shm_ptr;
    int elements = 0;
    while ((elements < totalContainersDropped))
    {
        FlourContainer *container = (FlourContainer *)temp;
        if (container->height != 0)
        {
            container->height -= 15;
            if (container->height < 0)
            {
                container->height = 0;
            }
            printf("Height of the container %d updated to %d\n", container->container_id, container->height);
        }
        temp += sizeof(FlourContainer);
        elements++;
    }
    if (sem_post(sem_containers) == -1)
    {
        perror("sem_post");
        exit(1);
    }
    if (sem_post(sem_data) == -1)
    {
        perror("sem_post");
        exit(1);
    }
    printf("\033[0;32mEnd Monitoring...\n\033[0m");
}

void signalHandler(int sig)
{
    if (sig == SIGALRM)
    {
        // printf("Monitor ALARM SIGNAL\n");
        updateHeights();
        alarm(period);
    }
    else if (sig == SIGUSR1)
    {
        // printf("Monitor USR1 SIGNAL\n");
        alarm(period);
    }
    else if (sig == SIGTSTP)
    {
        printf("Exiting monitor\n");
        exit(0);
    }
    else
    {
        printf("Unknown signal\n");
    }
}

void setupSignals()
{
    if (sigset(SIGALRM, signalHandler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }

    if (sigset(SIGUSR1, signalHandler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }

    if (sigset(SIGTSTP, signalHandler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
}

void open_shm_sem()
{
    cont_shmid = openSHM(SHM_PLANES, SHM_SIZE);
    if (cont_shmid == -1)
    {
        perror("openSHM Cargo Plane Error");
        exit(1);
    }
    cont_shm_ptr = (int *)mapSHM(cont_shmid, SHM_SIZE);
    if (cont_shm_ptr == NULL)
    {
        perror("mapSHM Cargo Plane Error");
        exit(1);
    }

    data_shmid = openSHM(SHM_DATA, SHM_DATA_SIZE);
    if (data_shmid == -1)
    {
        perror("openSHM Shared Data Error");
        exit(1);
    }
    data_shm_ptr = (int *)mapSHM(data_shmid, SHM_DATA_SIZE);
    if (data_shm_ptr == NULL)
    {
        perror("mapSHM Shared Data Error");
        exit(1);
    }

    sem_containers = sem_open(SEM_CONTAINERS, O_CREAT | O_RDWR, 0666, 1);
    if (sem_containers == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    sem_data = sem_open(SEM_DATA, O_CREAT | O_RDWR, 0666, 1);
    if (sem_data == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    printf("Monitor opened shared memory and semaphores\n");
}