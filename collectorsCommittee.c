#include "functions.c"
#include "local.h"

void setupSignals();
void signalHandler(int sig);
void open_shm_sem();
void collectContainers();

// shared data
int data_shmid;
int *data_shm_ptr;
sem_t *sem_data;
// safe area
int safe_shmid;
int *safe_shm_ptr;
sem_t *sem_safe;
// landed area
int landed_shmid;
int *landed_shm_ptr;
sem_t *sem_landed;

int committee_id;
int committee_size;
Collecter *collecters;
int period = 12;

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s <committee_id> <committee_size> <min_energy> <max_energy>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    committee_id = atoi(argv[1]);
    committee_size = atoi(argv[2]);
    int min_energy = atoi(argv[3]);
    int max_energy = atoi(argv[4]);
    collecters = (Collecter *)malloc(committee_size * sizeof(Collecter));
    // create the workers
    srand(time(NULL) ^ (getpid() << 16));
    int energy = rand() % (max_energy - min_energy + 1) + min_energy;
    for (int i = 0; i < committee_size; i++)
    {
        Collecter *collecter = malloc(sizeof(Collecter));
        collecter->committee_id = committee_id;
        collecter->energy = energy;
        collecter->alive = 1;
        collecters[i] = *collecter;
    }
    printf("Collectors Committee %d has been created\n", committee_id);
    setupSignals();
    open_shm_sem();
    while (1)
    {
        // sleep(5);
        // alarm(period);
        pause();
    }
}

void collectContainers()
{
    // read the containers in the landed area
    if (sem_wait(sem_data) == -1)
    {
        perror("waitSEM");
        exit(1);
    }
    // printf("Committee %d Locked Data SEM\n", committee_id);
    if (sem_wait(sem_landed) == -1)
    {
        perror("waitSEM");
        exit(1);
    }
    int totalLandedContainers = ((SharedData *)data_shm_ptr)->totalLandedContainers;
    int collectedContainers = ((SharedData *)data_shm_ptr)->cleectedContainers;
    int crashedContainers = ((SharedData *)data_shm_ptr)->numOfCrashedContainers;
    int droppedContainers = ((SharedData *)data_shm_ptr)->totalContainersDropped;
    int maxContainers = ((SharedData *)data_shm_ptr)->maxContainers;
    // printf("Committee %d Locked Landed SEM\n", committee_id);
    printf("\033[0;34mCollectors Committee %d is collecting containers...\n\033[0m", committee_id);
    if ((totalLandedContainers - collectedContainers) == 0)
    {
        printf("\033[0;31mNo containers to collect\n\033[0m");
    }
    else
    {
        int *temp = landed_shm_ptr;
        temp += sizeof(FlourContainer) * collectedContainers;
        FlourContainer *container = (FlourContainer *)temp;
        container->collected = 1;
        ((SharedData *)data_shm_ptr)->cleectedContainers++;
        printf("\033[0;32mContainer %d has been collected\n\033[0m", ((SharedData *)data_shm_ptr)->cleectedContainers);
        printf("State: | Quantity = %d | Height = %d\n | Crahsed = %d| Landed = %d | Collected = %d\n",
               container->quantity, container->height, container->crahshed, container->landed, container->collected);
        printf("\033[0;31mAt end of collection, Landed = %d | Collected = %d\n\033[0m",
               ((SharedData *)data_shm_ptr)->totalLandedContainers, ((SharedData *)data_shm_ptr)->cleectedContainers);
    }
    if (sem_post(sem_data) == -1)
    {
        perror("signalSEM");
        exit(1);
    }
    // printf("Committee %d Unlocked Data SEM\n", committee_id);
    if (sem_post(sem_landed) == -1)
    {
        perror("signalSEM");
        exit(1);
    }
    // printf("Committee %d Unlocked Landed SEM\n", committee_id);
    printf("\033[0;34mEnd Collecting...\n\033[0m");
    fflush(stdout);
}

void signalHandler(int sig)
{
    if (sig == SIGALRM)
    {
        collectContainers();
        alarm(period);
    }
    else if (sig == SIGUSR1)
    {
        alarm(period);
    }
    else if (sig == SIGTSTP)
    {
        // // unlink the shared memory and semaphores
        // if (shm_unlink(SHM_DATA) == -1)
        // {
        //     perror("shm_unlink data");
        //     exit(1);
        // }
        // if (shm_unlink(SHM_SAFE) == -1)
        // {
        //     perror("shm_unlink safe");
        //     exit(1);
        // }
        // if (shm_unlink(SHM_LANDED) == -1)
        // {
        //     perror("shm_unlink landed");
        //     exit(1);
        // }
        // if (sem_close(sem_data) == -1)
        // {
        //     perror("sem_close data");
        //     exit(1);
        // }
        // if (sem_close(sem_safe) == -1)
        // {
        //     perror("sem_close safe");
        //     exit(1);
        // }
        // if (sem_close(sem_landed) == -1)
        // {
        //     perror("sem_close landed");
        //     exit(1);
        // }
        printf("Exiting committee\n");
        exit(0);
    }
}

void setupSignals()
{
    if (sigset(SIGALRM, signalHandler) == SIG_ERR)
    {
        perror("sigaction");
        exit(1);
    }
}

void open_shm_sem()
{
    // Open the shared memory segment
    if ((data_shmid = shm_open(SHM_DATA, O_RDWR, 0666)) == -1)
    {
        perror("shm_open data");
        exit(1);
    }
    if ((safe_shmid = shm_open(SHM_SAFE, O_RDWR, 0666)) == -1)
    {
        perror("shm_open data");
        exit(1);
    }
    if ((landed_shmid = shm_open(SHM_LANDED, O_RDWR, 0666)) == -1)
    {
        perror("shm_open data");
        exit(1);
    }

    // Attach the shared memory segment
    if ((data_shm_ptr = mmap(0, SHM_DATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, data_shmid, 0)) == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }
    if ((landed_shm_ptr = mmap(0, SHM_LANDED_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, landed_shmid, 0)) == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }
    if ((safe_shm_ptr = mmap(0, SHM_SAFE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, safe_shmid, 0)) == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }

    // Open the semaphore
    if ((sem_data = sem_open(SEM_DATA, O_RDWR)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }
    if ((sem_landed = sem_open(SEM_LANDED, O_RDWR)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }
    if ((sem_safe = sem_open(SEM_SAFE, O_RDWR)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    printf("Collectors Committee opened shared memory and semaphores\n");
}