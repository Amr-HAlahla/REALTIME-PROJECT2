#include "functions.c"
#include "local.h"

void setupSignals();
void signalHandler(int sig);
void open_shm_sem();

int data_shmid;
int *data_shm_ptr;
sem_t *sem_data;
int cont_shmid;
int *cont_shm_ptr;
sem_t *sem_containers;
int safe_shmid;
int *safe_shm_ptr;
sem_t *sem_safe;

int committee_id;
int committee_size;
Collecter *collecters;

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
    // setupSignals();
    open_shm_sem();
    while (1)
    {
        // sleep(5);
        // alarm(period);
        pause();
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
    if ((cont_shmid = shm_open(SHM_PLANES, O_RDWR, 0666)) == -1)
    {
        perror("shm_open data");
        exit(1);
    }
    if ((safe_shmid = shm_open(SHM_SAFE, O_RDWR, 0666)) == -1)
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
    if ((cont_shm_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, cont_shmid, 0)) == MAP_FAILED)
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
    if ((sem_containers = sem_open(SEM_CONTAINERS, O_RDWR)) == SEM_FAILED)
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