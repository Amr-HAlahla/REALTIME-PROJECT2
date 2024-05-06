#include "local.h"
#include "functions.c"

void setupSignals();
void signalHandler(int sig);
void open_shm_sem();
void bagsDistribution();
void tryToKillWorker();
void replaceWorker();

// stage 2 shared data
int stage2_shmid;
int *stage2_shm_ptr;
sem_t *sem_stage2;
// families shared data
int families_shmid;
int *families_shm_ptr;
sem_t *sem_families;

int committee_id;
int committee_size;
int min_energy;
int max_energy;
Distributer **distributers;
int period;
int totalCapacity; // total capacity of all distributers
int killedWorkers = 0;
int random_victim;

int CRITICAL = 0;

int main(int argc, char *argv[])
{

    if (argc != 7)
    {
        fprintf(stderr, "Usage: %s <commitee_id> <commitee_size> <min_energy> <max_energy> <energy_per_trip> <capacity>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    committee_id = atoi(argv[1]);
    committee_size = atoi(argv[2]);
    min_energy = atoi(argv[3]);
    max_energy = atoi(argv[4]);
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
    CRITICAL = 1;
    /* Distribute bags to the families based on their needs and the capacity of the distributers */
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
            /* get the family data */
            Family *family = (Family *)(families_shm_ptr + i * sizeof(Family));
            families_process = family->pid;
            if (family->alive)
            {
                int neededBags = family->needed_bags;
                if (neededBags > 0)
                {
                    /* distribute bags to the family */
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
        if (done)
        {
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
                        killedWorkers++;
                    }
                }
            }
            // some families have received bags, send a signal to the families process to reorder them
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
    CRITICAL = 0;
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
    else if (sigset(SIGUSR1, signalHandler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
    else if (sigset(SIGUSR2, signalHandler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
}

void tryToKillWorker()
{
    printf("Distributers Commitee %d received SIGUSR1, occupation forces are trying to kill a worker\n", distributers[0]->committee_id);
    /* occupier wants to kill a worker, choose a random worker to kill
    the decision is based on the energy of the workers, it will decide if the worker will be killed or not
    */
    if (killedWorkers == committee_size)
    {
        printf("All workers in comittee %d have been killed.\n", distributers[0]->committee_id);
    }
    else
    {
        random_victim = rand() % committee_size;
        while (distributers[random_victim]->alive == 0)
        {
            random_victim = rand() % committee_size;
        }
        // now get the energy of this worker
        int energy = distributers[random_victim]->energy;
        // Decide if the worker will be killed based on their energy level
        // Probability of being killed is higher for workers with lower energy
        // Generate a random number between 0 and 10000 (100 squared)
        int chance = rand() % 10001;

        // Square the worker's energy level
        int energy_squared = energy * energy;

        // If the chance is less than the worker's squared energy, the worker survives
        if (chance < energy_squared)
        {
            printf("\033[0;32mWorker %d survived.\n\033[0m", random_victim);
        }
        // Otherwise, the worker is killed
        else
        {
            printf("\033[0;31mWorker %d was killed by the occupation forces.\n\033[0m", random_victim);
            // Code to remove the worker goes here
            distributers[random_victim]->alive = 0;
            killedWorkers++;
            // Notify parent of successful killing process.
            kill(getppid(), SIGUSR2);
        }
    }
}

void replaceWorker()
{
    sleep(3); // wait for the splitter worker to arrive to replace the killed worker
    // killed worker is going to be reaplced by a splitter worker.
    printf("Distributers Commitee %d received SIGUSR2, killed worker is going to be replaced by a splitter worker\n", committee_id);
    for (int i = 0; i < committee_size; i++)
    {
        if (distributers[i]->alive == 0)
        {
            distributers[i]->alive = 1;
            int energy = rand() % (max_energy - min_energy + 1) + min_energy;
            distributers[i]->energy = energy;
            printf("Worker %d is going to be replaced by a splitter worker\n", i);
            killedWorkers--;
            break;
        }
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
        printf("Distributers Commitee %d received SIGTSTP\n", distributers[0]->committee_id);
        if (CRITICAL)
        {
            if (sem_post(sem_stage2) == -1)
            {
                perror("sem_post");
            }
            if (sem_post(sem_families) == -1)
            {
                perror("sem_post");
            }
        }
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
