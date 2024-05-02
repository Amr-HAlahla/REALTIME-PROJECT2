#include "local.h"
#include "functions.c"

void setupSignals();
void signalHandler(int sig);
void open_shm_sem();
void splitContainers();

// safe area
int safe_shmid;
int *safe_shm_ptr;
sem_t *sem_safe;
// stage 2 shared data
int stage2_shmid;
int *stage2_shm_ptr;
sem_t *sem_stage2;

int period;
int splitter_id;

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <splitter_id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    period = rand() % (15 - 5 + 1) + 5;
    splitter_id = atoi(argv[1]);
    setupSignals();
    open_shm_sem();
    while (1)
    {
        pause();
    }
}

void splitContainers()
{
    if (sem_wait(sem_stage2) == -1)
    {
        perror("waitSEM");
        exit(1);
    }
    if (sem_wait(sem_safe) == -1)
    {
        perror("waitSEM");
        exit(1);
    }
    int numOfSplittedContainers = ((STAGE2_DATA *)stage2_shm_ptr)->numOfSplittedContainers;
    int numOfBags = ((STAGE2_DATA *)stage2_shm_ptr)->numOfBags;
    int numOFCollctedContainers = ((STAGE2_DATA *)stage2_shm_ptr)->numOFCollectedContainers;
    printf("--------------------------------------------\n");
    printf("\033[0;33mSplitter %d Splitting Containers...\n\033[0m", splitter_id);
    // printf("Number of collected containers = %d\n", numOFCollctedContainers);
    // printf("Number of splitted containers = %d\n", numOfSplittedContainers);
    // printf("Number Of Bags = %d\n", numOfBags);
    if (numOfSplittedContainers == numOFCollctedContainers)
    {
        printf("\033[0;32mNo containers to split\n\033[0m");
    }
    else
    {
        int *temp_ptr = safe_shm_ptr;
        temp_ptr += sizeof(FlourContainer) * numOfSplittedContainers;
        FlourContainer *Container = (FlourContainer *)temp_ptr;
        ((STAGE2_DATA *)stage2_shm_ptr)->numOfSplittedContainers++;
        ((STAGE2_DATA *)stage2_shm_ptr)->numOfBags += Container->quantity;
        printf("\033[0;33mSplitter %d have splitted a new %d Bags\n\033[0m", splitter_id, Container->quantity);
        printf("Number of collected containers = %d\n", ((STAGE2_DATA *)stage2_shm_ptr)->numOFCollectedContainers);
        printf("Number of containers splitted %d\n", ((STAGE2_DATA *)stage2_shm_ptr)->numOfSplittedContainers);
        printf("Total weight spliited = %d Kg of Bags\n", ((STAGE2_DATA *)stage2_shm_ptr)->numOfBags);
        // printf("Number of containers splitted %d", ((STAGE2_DATA *)stage2_shm_ptr)->numOfSplittedContainers);
        // printf("Number of weight spliited = %d Kg of Bags", ((STAGE2_DATA *)stage2_shm_ptr)->numOfBags);
    }
    printf("--------------------------------------------\n");
    printf("\033[0;33mSplitter %d is Done Splitting\n\033[0m", splitter_id);
    if (sem_post(sem_stage2) == -1)
    {
        perror("postSEM");
        exit(1);
    }
    if (sem_post(sem_safe) == -1)
    {
        perror("postSEM");
        exit(1);
    }
}

void open_shm_sem()
{
    if ((safe_shmid = shm_open(SHM_SAFE, O_RDWR, 0666)) == -1)
    {
        perror("shm_open data");
        exit(1);
    }
    if ((stage2_shmid = shm_open(SHM_STAGE2, O_RDWR, 0666)) == -1)
    {
        perror("shm_open data");
        exit(1);
    }

    if ((safe_shm_ptr = mmap(0, SHM_SAFE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, safe_shmid, 0)) == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }
    if ((stage2_shm_ptr = mmap(0, SHM_STAGE2_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, stage2_shmid, 0)) == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }

    if ((sem_safe = sem_open(SEM_SAFE, O_RDWR)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }
    if ((sem_stage2 = sem_open(SEM_STAGE2, O_RDWR)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    printf("Splitter %d Committee opened shared memory and semaphores\n", splitter_id);
}

void setupSignals()
{
    if (sigset(SIGALRM, signalHandler) == SIG_ERR)
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

void signalHandler(int sig)
{
    if (sig == SIGALRM)
    {
        splitContainers();
        alarm(period);
    }
    else if (sig == SIGTSTP)
    {
        printf("Splitter received SIGTSTP\n");
        printf("\033[0;31mExiting splitter\n\033[0m");
        exit(0);
    }
    else
    {
        printf("Unknown signal\n");
    }
}