#include "functions.c"
#include "local.h"

void setupSignals();
void signalHandler(int sig);
void updateHeights();
void open_shm_sem();

int cont_shmid;
int *cont_shm_ptr;
sem_t *sem_containers;
int data_shmid;
int *data_shm_ptr;
sem_t *sem_data;

int totalContainersDropped = 0;

int period = 5;         // Period for updating heights
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
    if (sem_wait(sem_data) == -1)
    {
        perror("waitSEM");
        exit(1);
    }
    int totalContainersDropped = ((SharedData *)data_shm_ptr)->totalContainersDropped;
    int totalLandedContainers = ((SharedData *)data_shm_ptr)->totalLandedContainers;
    int numOfCrashedContainers = ((SharedData *)data_shm_ptr)->numOfCrashedContainers;
    int collectedContainers = ((SharedData *)data_shm_ptr)->cleectedContainers;
    if (sem_wait(sem_containers) == -1)
    {
        perror("waitSEM");
        exit(1);
    }
    // choose a target container to crash (between 0 and totalContainersDropped)
    printf("\033[0;32mStart Monitoring...\n\033[0m");
    int *temp = cont_shm_ptr;
    printf("Total containers dropped = %d\n", totalContainersDropped);
    printf("Total crashed containers = %d\n", numOfCrashedContainers);
    printf("Total landed containers = %d\n", totalLandedContainers);
    printf("Total collected containers = %d\n", collectedContainers);
    int lower_bound = totalLandedContainers + numOfCrashedContainers;
    printf("Lower bound = %d\n", lower_bound);
    if (lower_bound >= totalContainersDropped)
    {
        targetContainer = -1;
    }
    else
    {
        targetContainer = rand() % (totalContainersDropped - lower_bound) + lower_bound;
    }
    printf("Target container %d\n", targetContainer);
    int elements = 0;
    while (elements < totalContainersDropped)
    {
        /* check if the container is collected or landed or crashed */
        FlourContainer *container = (FlourContainer *)temp;
        if (container->collected == 1 || container->landed == 1 || container->crahshed == 1)
        {
            // printf("\033[0;31mContainer %d is collected or landed or crashed\n\033[0m", elements);
            temp += sizeof(FlourContainer);
            elements++;
            continue; // skip the container
        }
        else /* the container is not collected or landed or crashed */
        {
            // monitor the height of the container
            container->height -= 10;
            if (container->height <= 0)
            {
                container->height = 0;
                container->landed = 1;
                printf("\033[0;31mThe container %d has reached the ground with quantity %d %d\n\033[0m",
                       elements, container->quantity, container->height);
                ((SharedData *)data_shm_ptr)->totalLandedContainers++;
            }
            printf("Height of the container %d updated to %d\n", elements, container->height);
            /* check if the container is the target container, and ensure it hasn't landed in this round */
            if (targetContainer == elements)
            {
                if (container->landed == 0)
                {
                    // generate a random prob number between 0 and 100
                    int prob = rand() % 101;
                    if (prob < propThreshold)
                    {
                        // printf("\033[0;31mProb = %d, the container %d will be crahsed\n\033[0m", prob, elements);
                        // the target is crashed, check the height
                        if (container->height > damageThreshold)
                        {
                            // the container will be removed totally
                            container->crahshed = 1;
                            container->quantity = 0;
                            printf("\033[0;31mContainer %d at height %d has been totally damaged\n\033[0m",
                                   elements, container->height);
                            ((SharedData *)data_shm_ptr)->numOfCrashedContainers++;
                        }
                        else
                        {
                            // the container is partially damaged by a ration depends on its height
                            int tempQuantity = container->quantity;
                            // generate a ration between (1 and 100)% of the height
                            float ratio = (container->height + 1) / 100.0;
                            printf("Damage ratio = %0.2f\n", ratio);
                            container->quantity = (int)container->quantity * (1.0 - ratio);
                            printf("\033[0;31mContainer %d at height %d has been partially damaged: quantity from %d to %d\n\033[0m",
                                   elements, container->height, tempQuantity, container->quantity);
                        }
                    }
                    else
                    {
                        printf("\033[0;31mContainer %d is safe, prob = %d is greater than threshold = %d\n\033[0m", elements, prob, propThreshold);
                    }
                }
                else
                {
                    printf("\033[0;31mContainer %d has reached the ground, will not be damaged\n\033[0m", elements);
                }
            }
            temp += sizeof(FlourContainer);
            elements++;
        }
    }
    printf("Containers | Dropped = %d | Landed = %d | Collected=  %d | Crahsed = %d\n", ((SharedData *)data_shm_ptr)->totalContainersDropped,
           ((SharedData *)data_shm_ptr)->totalLandedContainers, ((SharedData *)data_shm_ptr)->cleectedContainers, ((SharedData *)data_shm_ptr)->numOfCrashedContainers);
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
        updateHeights();
        alarm(period);
    }
    else if (sig == SIGUSR1)
    {
        alarm(period);
    }
    else if (sig == SIGTSTP)
    {
        printf("Monitor received SIGTSTP\n");
        printf("Exiting monitor\n");
        // exit(0);
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