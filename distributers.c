#include "local.h"
#include "functions.c"

void setupSignals();
void signalHandler(int sig);
void open_shm_sem();
void bagsDistribution();

// safe area
// int safe_shmid;
// int *safe_shm_ptr;
// sem_t *sem_safe;
// stage 2 shared data
int stage2_shmid;
int *stage2_shm_ptr;
sem_t *sem_stage2;

// int committee_id;
int committee_size;
// int capacity;
Distributer **distributers;
int period;
int totalCapacity; // total capacity of all distributers

int main(int argc, char *argv[])
{

    if (argc != 7)
    {
        fprintf(stderr, "Usage: %s <commitee_id> <commitee_size> <min_energy> <max_energy> <energy_per_trip> <capacity>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int committee_id = atoi(argv[1]);
    committee_size = atoi(argv[2]);
    int min_energy = atoi(argv[3]);
    int max_energy = atoi(argv[4]);
    int energy_per_trip = atoi(argv[5]);
    int capacity = atoi(argv[6]);
    distributers = malloc(sizeof(Distributer *) * committee_size);
    totalCapacity = committee_size * capacity;
    period = rand() % (12 - 5 + 1) + 5;
    // create the workers
    srand(time(NULL) ^ (getpid() << 16));
    printf("Creating Workers, Energy Per Trip = %d\n", energy_per_trip);
    for (int i = 0; i < committee_size; i++)
    {
        Distributer *distributer = malloc(sizeof(Distributer));
        int energy = rand() % (max_energy - min_energy + 1) + min_energy;
        distributer->committee_id = committee_id;
        distributer->energy = energy;
        distributer->alive = 1;
        distributer->energy_per_trip = energy_per_trip;
        distributer->capacity = capacity;
        // totalCapacity += capacity;
        distributers[i] = distributer;
        printf("Worker %d | Energy = %d | Capacity = %d\n", i, energy, capacity);
    }
    printf("Distributers Commitee %d has been created with total capacity of %d\n", committee_id, totalCapacity);
    setupSignals();
    open_shm_sem();
    while (1)
    {
        pause();
    }
}

void bagsDistribution()
{
    // access the shared data of stage 2.
    if (sem_wait(sem_stage2) == -1)
    {
        perror("sem_wait");
        exit(1);
    }
    STAGE2_DATA *stage2_data = (STAGE2_DATA *)stage2_shm_ptr;
    int numOfBags = stage2_data->numOfBags;                 // available bags in the storage
    int numOfDistriutedBags = stage2_data->distributedBags; // distributed bags
    if (numOfBags <= 0)
    {
        printf("\033[0;31mNo more bags to distribute\033[0m\n");
    }
    else
    {
        // calculate the bags to distribute
        int bagsToDistribute = totalCapacity > numOfBags ? numOfBags : totalCapacity;
        // distribute the bags
        ((STAGE2_DATA *)stage2_shm_ptr)->distributedBags += bagsToDistribute;
        ((STAGE2_DATA *)stage2_shm_ptr)->numOfBags -= bagsToDistribute;
        printf("\033[0;34mDistributers Commitee %d is distributing %d bags\n\033[0m", distributers[0]->committee_id, bagsToDistribute);
        // update the energy of the workers
        for (int i = 0; i < committee_size; i++)
        {
            if (distributers[i]->alive)
            {
                distributers[i]->energy -= distributers[i]->energy_per_trip;
                if (distributers[i]->energy <= 0)
                {
                    // printf("\033[0;31mWorker %d has died\033[0m\n", i);
                    distributers[i]->alive = 0;
                    distributers[i]->energy = 0;
                }
            }
        }
    }
    if (sem_post(sem_stage2) == -1)
    {
        perror("sem_post");
        exit(1);
    }
}

void setupSignals()
{
    if (sigset(SIGALRM, signalHandler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
    else if (sigset(SIGTSTP, signalHandler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
}

void signalHandler(int sig)
{
    if (sig == SIGALRM)
    {
        bagsDistribution();
        alarm(period);
    }
    else if (sig == SIGTSTP)
    {
        printf("Distributers Commitee %d received SIGTSTP\n", distributers[0]->committee_id);
        for (int i = 0; i < committee_size; i++)
        {
            printf("Worker %d | Energy = %d | Alive = %d\n", i, distributers[i]->energy, distributers[i]->alive);
        }
        printf("Distributers Commitee %d is terminated\n", distributers[0]->committee_id);
        for (int i = 0; i < committee_size; i++)
        {
            free(distributers[i]);
        }
        free(distributers);
        exit(0);
    }
}
void open_shm_sem()
{
    // open stage 2 shared memory
    if ((stage2_shmid = shm_open(SHM_STAGE2, O_RDWR, 0666)) == -1)
    {
        perror("shm_open");
        exit(1);
    }
    if ((stage2_shm_ptr = mmap(NULL, SHM_STAGE2_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, stage2_shmid, 0)) == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }
    if ((sem_stage2 = sem_open(SEM_STAGE2, O_RDWR)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }
}
