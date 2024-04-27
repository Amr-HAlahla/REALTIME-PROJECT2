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

int period = 7;         // Period for updating heights
int targetContainer;    // Container to be crashed
int propThreshold = 30; // above threshold => crash the container
/* above threshold meters => all the content of the container is damaged, and the container is removed
    below threshold meters => partial damage   */
int damageThreshold = 65;
int main(int argc, char *argv[])
{
    srand(time(NULL) ^ (getpid() << 16)); // Seed random generator based on planeID
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
    // printf("MONITOR Process Passed sem_data\n");
    int totalContainersDropped = ((SharedData *)data_shm_ptr)->totalContainersDropped;
    if (sem_wait(sem_containers) == -1)
    {
        perror("waitSEM");
        exit(1);
    }
    // printf("MONITOR Process Passed sem_containers\n");
    // choose a target container to crash (between 0 and totalContainersDropped)
    printf("\033[0;32mStart Monitoring...\n\033[0m");
    int *temp = cont_shm_ptr;
    int elements = 0;
    printf("Total containers dropped = %d\n", totalContainersDropped);
    targetContainer = rand() % (totalContainersDropped + 1);
    printf("Target container %d\n", targetContainer);
    while ((elements < totalContainersDropped) && totalContainersDropped > 0)
    {
        /* check if the container is the target container */
        if (elements == targetContainer)
        {
            FlourContainer *container = (FlourContainer *)temp;
            // generate a random prob number between 0 and 100
            int prob = rand() % 101;
            if (prob < propThreshold)
            {
                printf("\033[0;31mProb = %d, the container %d will be crashed\n\033[0m", prob, container->container_id);
                // the target is crashed, check the height
                if (container->height > damageThreshold)
                {
                    // the container is totally damaged
                    container->quantity = 0;
                    printf("\033[0;31mContainer %d at height %d and has been crashed totally: quantity = %d\n\033[0m",
                           container->container_id, container->height, container->quantity);
                    ((SharedData *)data_shm_ptr)->totalContainersDropped--;
                }
                else
                {
                    // the container is partially damaged by a ration depends on its height
                    int tempQuantity = container->quantity;
                    // generate a ration between (1 and 100)% of the height
                    float ratio = (container->height + 1) / 100.0;
                    printf("Damage ratio = %0.2f\n", ratio);
                    container->quantity = (int)container->quantity * (1 - ratio);
                    printf("\033[0;31mContainer %d at height %d has been partially crashed: quantity from %d to %d\n\033[0m",
                           container->container_id, container->height, tempQuantity, container->quantity);
                    if (container->quantity == 0)
                    {
                        ((SharedData *)data_shm_ptr)->totalContainersDropped--;
                    }
                }
            }
            else
            {
                printf("Container %d is safe, prob = %d is greater than %d\n", container->container_id, prob, propThreshold);
            }
        }
        // now update the height of the container
        FlourContainer *container = (FlourContainer *)temp;
        if (container->height != 0 && container->quantity != 0)
        {
            container->height -= 10;
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
    fflush(stdout);
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

    sem_containers = sem_open(SEM_CONTAINERS, O_RDWR);
    if (sem_containers == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    sem_data = sem_open(SEM_DATA, O_RDWR);
    if (sem_data == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    printf("Monitor opened shared memory and semaphores\n");
}