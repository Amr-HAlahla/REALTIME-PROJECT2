#include "functions.c"
#include "local.h"

void setupSignals();
void signalHandler(int sig);
void open_shm_sem();
void collectContainers();
void tryToKillWorker();
void replaceWorker();

// shared data
int data_shmid;
int *data_shm_ptr;
sem_t *sem_data;
// safe area
int safe_shmid;
int *safe_shm_ptr;
sem_t *sem_safe;
// stage 2 shared data
int stage2_shmid;
int *stage2_shm_ptr;
sem_t *sem_stage2;
// containers shared data
int cont_shmid;
int *cont_shm_ptr;
sem_t *sem_containers;

int committee_id;
int committee_size;
int min_energy;
int max_energy;
Collecter **collecters;
int period;
int killedWorker = 0;
int CRITICAL = 0;

int main(int argc, char *argv[])
{
    if (argc != 6)
    {
        fprintf(stderr, "Usage: %s <committee_id> <committee_size> <min_energy> <max_energy> <energy_per_trip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    committee_id = atoi(argv[1]);
    committee_size = atoi(argv[2]);
    min_energy = atoi(argv[3]);
    max_energy = atoi(argv[4]);
    int energy_per_trip = atoi(argv[5]);
    collecters = malloc(sizeof(Collecter *) * committee_size);
    period = rand() % (15 - 8 + 1) + 8;
    // create the workers
    srand(time(NULL) ^ (getpid() << 16));
    for (int i = 0; i < committee_size; i++)
    {
        Collecter *collecter = malloc(sizeof(Collecter));
        int energy = rand() % (max_energy - min_energy + 1) + min_energy;
        collecter->committee_id = committee_id;
        collecter->energy = energy;
        collecter->alive = 1;
        collecter->energy_per_trip = energy_per_trip;
        collecters[i] = collecter;
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
    CRITICAL = 1;
    int totalLandedContainers = ((SharedData *)data_shm_ptr)->totalLandedContainers;
    int collectedContainers = ((SharedData *)data_shm_ptr)->cleectedContainers;
    int crashedContainers = ((SharedData *)data_shm_ptr)->numOfCrashedContainers;
    int droppedContainers = ((SharedData *)data_shm_ptr)->totalContainersDropped;
    printf("\033[0;34mCollectors Committee %d is collecting containers...\n\033[0m", committee_id);
    if ((totalLandedContainers - collectedContainers) == 0)
    {
        printf("\033[0;31mNo containers to collect\n\033[0m");
    }
    else
    {
        /* read the containers in the landed area */
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
        /* write the container to the safe area */
        int *temp2 = safe_shm_ptr;
        temp2 += sizeof(FlourContainer) * (((SharedData *)data_shm_ptr)->cleectedContainers);
        container->collected = 1;
        memcpy(temp2, container, sizeof(FlourContainer));
        ((SharedData *)data_shm_ptr)->cleectedContainers++;
        printf("||State: | Quantity = %d | Height = %d | Crahsed = %d| Landed = %d | Collected = %d ||\n",
               container->quantity, container->height, container->crahshed, container->landed, container->collected);
        // printf("\033[0;31mAt end of collection | Landed = %d | Collected = %d \n\033[0m",
        //        ((SharedData *)data_shm_ptr)->totalLandedContainers, ((SharedData *)data_shm_ptr)->cleectedContainers);
        ((STAGE2_DATA *)stage2_shm_ptr)->numOFCollectedContainers++;
        for (int i = 0; i < committee_size; i++)
        {
            /* decrease the energy of the workers */
            if (collecters[i]->alive)
            {
                collecters[i]->energy -= collecters[i]->energy_per_trip;
                if (collecters[i]->energy <= 0)
                {
                    collecters[i]->energy = 0;
                    collecters[i]->alive = 0;
                    killedWorker++;
                    printf("\033[0;31mWorker %d has died\n\033[0m", i);
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
    CRITICAL = 0;
    printf("\033[0;34mEnd Collecting...\n\033[0m");
    fflush(stdout);
}

void tryToKillWorker()
{
    printf("Collecting Committee %d received SIGUSR1, occupaion forces are trying to kill a worker\n", committee_id);
    if (killedWorker == committee_size)
    {
        printf("All workers in committee %d are dead\n", committee_id);
    }
    else
    {
        /* choose a random worker to kill */
        int random_worker = rand() % committee_size;
        while (!collecters[random_worker]->alive)
        {
            random_worker = rand() % committee_size;
        }
        int energy = collecters[random_worker]->energy;
        // Generate a random number between 0 and 10000 (100 squared)
        int chance = rand() % 10001;
        // Square the worker's energy level
        int energy_squared = energy * energy;
        // If the chance is less than the worker's squared energy, the worker survives
        if (chance < energy_squared)
        {
            printf("\033[0;32mWorker %d survived.\n\033[0m", random_worker);
        }
        else
        {
            collecters[random_worker]->alive = 0;
            killedWorker++;
            printf("Worker %d has been killed by the occupation forces\n", random_worker);
            // notify parent that a worker has been killed
            kill(getppid(), SIGUSR2);
        }
    }
}
void replaceWorker()
{
    sleep(3); // wait for the worker to be replaced
    printf("Committee %d received SIGUSR2, killed worker has been replaced\n", committee_id);
    // find a died worker and make him alive and with new energy level
    for (int i = 0; i < committee_size; i++)
    {
        if (!collecters[i]->alive)
        {
            /* replace the killed worker */
            int energy = rand() % (max_energy - min_energy + 1) + min_energy;
            collecters[i]->energy = energy;
            collecters[i]->alive = 1;
            printf("Worker %d has been replaced with energy %d\n", i, energy);
            killedWorker--;
            break;
        }
    }
}

void signalHandler(int sig)
{
    if (sig == SIGALRM)
    {
        collectContainers();
        /* always update the period, to get a new random order for the committee */
        period = rand() % (15 - 8 + 1) + 8;
        alarm(period);
    }
    else if (sig == SIGUSR1)
    {
        tryToKillWorker();
    }
    else if (sig == SIGUSR2)
    {
        replaceWorker();
    }
    else if (sig == SIGTSTP)
    {
        printf("Committee %d received SIGTSTP\n", committee_id);
        if (CRITICAL == 1)
        {
            if (sem_post(sem_data) == -1)
            {
                perror("signalSEM");
            }
            if (sem_post(sem_containers) == -1)
            {
                perror("signalSEM");
            }
            if (sem_post(sem_safe) == -1)
            {
                perror("signalSEM");
            }
            if (sem_post(sem_stage2) == -1)
            {
                perror("signalSEM");
            }
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
    else
    {
        printf("Unknown signal\n");
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
    if (sigset(SIGUSR2, signalHandler) == SIG_ERR)
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