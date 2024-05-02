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
int stage2_shmid;
int *stage2_shm_ptr;
sem_t *sem_stage2;
int cont_shmid;
int *cont_shm_ptr;
sem_t *sem_containers;

int committee_id;
int committee_size;
Collecter **collecters;
int period;

int main(int argc, char *argv[])
{
    if (argc != 6)
    {
        fprintf(stderr, "Usage: %s <committee_id> <committee_size> <min_energy> <max_energy> <energy_per_trip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    committee_id = atoi(argv[1]);
    committee_size = atoi(argv[2]);
    int min_energy = atoi(argv[3]);
    int max_energy = atoi(argv[4]);
    int energy_per_trip = atoi(argv[5]);
    collecters = malloc(sizeof(Collecter *) * committee_size);
    period = rand() % (15 - 8 + 1) + 8;
    // create the workers
    srand(time(NULL) ^ (getpid() << 16));
    // printf("Creating Workers, Energy Per Trip = %d\n", energy_per_trip);
    for (int i = 0; i < committee_size; i++)
    {
        Collecter *collecter = malloc(sizeof(Collecter));
        int energy = rand() % (max_energy - min_energy + 1) + min_energy;
        collecter->committee_id = committee_id;
        collecter->energy = energy;
        collecter->alive = 1;
        collecter->energy_per_trip = energy_per_trip;
        collecters[i] = collecter;
        // printf("Worker %d | Energy = %d\n", i, energy);
    }
    printf("Collectors Committee %d has been created\n", committee_id);
    setupSignals();
    open_shm_sem();
    while (1)
    {
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
    if (sem_wait(sem_containers) == -1)
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
        int *temp = cont_shm_ptr;
        int index;
        for (index = 0; index < droppedContainers; index++)
        {
            FlourContainer *container = (FlourContainer *)temp;
            if (!container->landed || container->collected || container->crahshed)
            {
                temp += sizeof(FlourContainer);
                continue; // skip the container
            }
            break; // found a container to collect
        }
        FlourContainer *container = (FlourContainer *)temp;
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
        // write the container to the safe area
        int *temp2 = safe_shm_ptr;
        temp2 += sizeof(FlourContainer) * (((SharedData *)data_shm_ptr)->cleectedContainers);
        container->collected = 1;
        /* write the container to the safe area */
        memcpy(temp2, container, sizeof(FlourContainer));
        ((SharedData *)data_shm_ptr)->cleectedContainers++;
        printf("\033[0;32mContainer %d has been collected to the safe area as number %d\n\033[0m", index, ((SharedData *)data_shm_ptr)->cleectedContainers);
        printf("||State: | Quantity = %d | Height = %d | Crahsed = %d| Landed = %d | Collected = %d ||\n",
               container->quantity, container->height, container->crahshed, container->landed, container->collected);
        // printf("\033[0;31mAt end of collection | Landed = %d | Collected = %d \n\033[0m",
        //        ((SharedData *)data_shm_ptr)->totalLandedContainers, ((SharedData *)data_shm_ptr)->cleectedContainers);
        ((STAGE2_DATA *)stage2_shm_ptr)->numOFCollectedContainers++;
        for (int i = 0; i < committee_size; i++)
        {
            if (collecters[i]->alive)
            {
                collecters[i]->energy -= collecters[i]->energy_per_trip;
                if (collecters[i]->energy <= 0)
                {
                    collecters[i]->energy = 0;
                    collecters[i]->alive = 0;
                }
            }
        }
        if (sem_post(sem_stage2) == -1)
        {
            perror("signalSEM");
            exit(1);
        }
        if (sem_post(sem_safe) == -1)
        {
            perror("signalSEM");
            exit(1);
        }
    }
    if (sem_post(sem_data) == -1)
    {
        perror("signalSEM");
        exit(1);
    }
    if (sem_post(sem_containers) == -1)
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
        /* always update the period, to get a new random order for the committee */
        period = rand() % (15 - 8 + 1) + 8;
        collectContainers();
        alarm(period);
    }
    else if (sig == SIGUSR1)
    {
        alarm(period);
    }
    else if (sig == SIGTSTP)
    {
        printf("Committee %d received SIGTSTP\n", committee_id);
        printf("The energy of workers at the end of the day\n");
        for (int i = 0; i < committee_size; i++)
        {
            printf("Worker %d | Energy = %d | Alive = %d\n", i, collecters[i]->energy, collecters[i]->alive);
        }
        // free the allocated memory
        for (int i = 0; i < committee_size; i++)
        {
            free(collecters[i]);
        }
        free(collecters);
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
    if (sigset(SIGUSR1, signalHandler) == SIG_ERR)
    {
        perror("sigaction");
        exit(1);
    }
    if (sigset(SIGTSTP, signalHandler) == SIG_ERR)
    {
        perror("sigaction");
        exit(1);
    }
}

void open_shm_sem()
{
    // Open the shared memory segment
    if ((cont_shmid = shm_open(SHM_PLANES, O_RDWR, 0666)) == -1)
    {
        perror("shm_open data");
        exit(1);
    }
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
    if ((stage2_shmid = shm_open(SHM_STAGE2, O_RDWR, 0666)) == -1)
    {
        perror("shm_open data");
        exit(1);
    }

    // Attach the shared memory segment
    if ((cont_shm_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, cont_shmid, 0)) == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }
    if ((data_shm_ptr = mmap(0, SHM_DATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, data_shmid, 0)) == MAP_FAILED)
    {
        perror("mmap");
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

    // Open the semaphore
    if ((sem_containers = sem_open(SEM_CONTAINERS, O_RDWR)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }
    if ((sem_data = sem_open(SEM_DATA, O_RDWR)) == SEM_FAILED)
    {
        perror("sem_open");
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

    printf("Collectors Committee opened shared memory and semaphores\n");
}