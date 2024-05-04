#include "local.h"
#include "functions.c"

void setupSignals();
void signalHandler(int sig);
void open_shm_sem();
void bagsDistribution();

// stage 2 shared data
int stage2_shmid;
int *stage2_shm_ptr;
sem_t *sem_stage2;
// families shared data
int families_shmid;
int *families_shm_ptr;
sem_t *sem_families;

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
    int NUM_OF_FAMILIES = stage2_data->NUM_OF_FAMILIES;
    if (numOfBags <= 0)
    {
        printf("\033[0;31mNo more bags to distribute\033[0m\n");
    }
    else
    {
        // update the families data
        if (sem_wait(sem_families) == -1)
        {
            perror("sem_wait");
            exit(1);
        }
        // calculate the total capacity of the distributers
        for (int i = 0; i < committee_size; i++)
        {
            if (distributers[i]->alive)
            {
                // total amount of bags that can be distributed by the committee
                totalCapacity += distributers[i]->capacity;
            }
        }
        int bagsToDistribute = totalCapacity > numOfBags ? numOfBags : totalCapacity; // bags to distribute by this committee
        int done = 0;
        pid_t families_process;
        for (int i = 0; i < NUM_OF_FAMILIES; i++)
        {
            Family *family = (Family *)(families_shm_ptr + i * sizeof(Family));
            families_process = family->pid; // get the process id of the family
            if (family->alive)
            {
                int neededBags = family->needed_bags;
                if (neededBags > 0)
                {
                    int bags = neededBags > bagsToDistribute ? bagsToDistribute : neededBags;
                    family->needed_bags -= bags;
                    family->history += bags;
                    bagsToDistribute -= bags;
                    float update_ratio = (float)family->needed_bags / 100.0;
                    if (update_ratio > 1.0)
                    {
                        update_ratio = 1.0;
                    }
                    family->starvation_level -= family->starvation_level * (1 - update_ratio);
                    printf("\033[0;32mFamily %d has received %d bags\n\033[0m", i, bags);
                    done++;
                    // update the stage 2 data
                    ((STAGE2_DATA *)stage2_shm_ptr)->distributedBags += bags;
                    ((STAGE2_DATA *)stage2_shm_ptr)->numOfBags -= bags;
                }
            }
            if (bagsToDistribute <= 0)
            {

                break; // all bags have been distributed by this committee
            }
        }
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
        if (done)
        {
            // some families have received bags, send a signal to the families to reorder them
            if (kill(families_process, SIGUSR1) == -1)
            {
                perror("kill");
                exit(1);
            }
        }
        if (sem_post(sem_families) == -1)
        {
            perror("sem_post");
            exit(1);
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
        period = rand() % (12 - 5 + 1) + 5; // get a new random period
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

    // open families shared memory
    if ((families_shmid = shm_open(FAMILIES_SHM, O_RDWR, 0666)) == -1)
    {
        perror("shm_open");
        exit(1);
    }
    if ((families_shm_ptr = mmap(NULL, FAMILIES_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, families_shmid, 0)) == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }
    if ((sem_families = sem_open(SEM_FAMILIES, O_RDWR)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }
}
