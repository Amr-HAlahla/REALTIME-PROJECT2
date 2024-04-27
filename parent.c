#include "local.h"
#include "functions.c"

// for configuration.txt
typedef struct
{
    int numCargoPlanes;
    int minContainersPerPlane, maxContainersPerPlane;
    int minDropFrequency, maxDropFrequency;
    int minRefillPeriod, maxRefillPeriod;
    int numCollectingCommittees;
    int workersPerCommittee;
    int missileHitProbability;
    int bagsPerTrip;
    int starvationRateThreshold;
    int simulationDurationThreshold;
    int familyDeathRateThreshold;
    int planeCrashThreshold;
    int containerShotDownThreshold;
    int collectingWorkerMartyrThreshold;
    int distributingWorkerMartyrThreshold;
    int familyStarvationDeathThreshold;
} Config;

Config config;
pid_t cargoPlanes[MAX_CARGOPLANES];
int containersPerPlane[MAX_CARGOPLANES];

int shmid_planes;
int shmid_data;
int *containers_shm;
int *data_shm;
sem_t *sem_containers;
sem_t *sem_data;

// Function Prototypes:
void loadConfiguration(const char *, Config *);
void initialize_cargo_plane();
void initialize_shared_data();
void setupSignals();
void signalHandler(int sig);
void create_shm_sem();

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <configuration_file_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    loadConfiguration(argv[1], &config);
    setupSignals();
    create_shm_sem();
    initialize_shared_data();

    pid_t pid;
    char arg1[10], arg2[10], arg3[10], arg4[10], arg5[10], arg6[10], arg7[10];

    printf("Creating Cargo Planes:\n");
    for (int i = 0; i < config.numCargoPlanes; i++)
    {
        int numContainers = rand() % (config.maxContainersPerPlane - config.minContainersPerPlane + 1) + config.minContainersPerPlane;
        sprintf(arg1, "%d", i + 1); // Plane ID
        sprintf(arg2, "%d", numContainers);
        sprintf(arg3, "%d", config.minDropFrequency);
        sprintf(arg4, "%d", config.maxDropFrequency);
        sprintf(arg5, "%d", config.minRefillPeriod);
        sprintf(arg6, "%d", config.maxRefillPeriod);
        containersPerPlane[i] = numContainers;

        pid = fork();
        if (pid == -1)
        {
            perror("fork");
            exit(1);
        }

        if (pid == 0)
        { // Child process
            execl("./cargoPlane", "cargoPlane", arg1, arg2, arg3, arg4, arg5, arg6, NULL);
            perror("execl");
            exit(1);
        }
        else
        {
            cargoPlanes[i] = pid;
            // printf("Cargo Plane %d created with %d containers\n", i + 1, numContainers);
        }
    }
    sleep(5);
    for (int i = 0; i < config.numCargoPlanes; i++)
    {
        kill(cargoPlanes[i], SIGUSR1); // Start dropping containers
    }
    // Wait for all cargo planes to finish
    for (int i = 0; i < config.numCargoPlanes; i++)
    {
        waitpid(cargoPlanes[i], NULL, 0);
    }
    return 0;
}

void initialize_shared_data()
{
    if (sem_wait(sem_data) == -1)
    {
        perror("sem_wait");
        exit(1);
    }
    SharedData *data = (SharedData *)data_shm;
    data->totalContainersDropped = 0;
    data->cleectedContainers = 0;
    data->numOfCargoPlanes = config.numCargoPlanes;
    data->planesDropped = 0;
    if (sem_post(sem_data) == -1)
    {
        perror("sem_post");
        exit(1);
    }
}

void signalHandler(int sig)
{
    if (sig == SIGUSR1)
    {
        if (sem_wait(sem_data) == -1)
        {
            perror("sem_wait");
            exit(1);
        }
        int totalContainersDropped = ((SharedData *)data_shm)->totalContainersDropped;
        int cleectedContainers = ((SharedData *)data_shm)->cleectedContainers;
        printf("Parent Entered data critical section\n");
        if (sem_wait(sem_containers) == -1)
        {
            perror("sem_wait");
            exit(1);
        }
        printf("Parent entered containers critical section\n");
        printf("==========================================\n");
        int *temp = containers_shm;
        int i = 0;
        int summation = 0;
        for (int i = 0; i < config.numCargoPlanes; i++)
        {
            summation += containersPerPlane[i];
        }
        printf("Total containers dropped must be %d\n", summation);
        printf("Total containers dropped: %d\n", totalContainersDropped);
        // read containers of the first cargo plane from shared memory
        while ((totalContainersDropped > 0))
        {
            if (((FlourContainer *)temp)->quantity != 0)
            {
                printf("Container %d has been collected at address %p\n", ((FlourContainer *)temp)->container_id, temp);
                printf("\033[0;32mQuantity: %d and height: %d\n\033[0m", ((FlourContainer *)temp)->quantity, ((FlourContainer *)temp)->height);
                cleectedContainers++;
                totalContainersDropped--;
                ((FlourContainer *)temp)->height = 0;
                ((FlourContainer *)temp)->quantity = 0;
                i++;
            }
            // update the pointer to the next container
            temp += sizeof(FlourContainer);
        }
        printf("After collection process, Total containers dropped: %d\n", totalContainersDropped);
        // update shared data with the new values
        ((SharedData *)data_shm)->totalContainersDropped = totalContainersDropped;
        ((SharedData *)data_shm)->cleectedContainers = cleectedContainers;
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
        printf("Parent exited the critical section\n");
        // closeSHM(containers_shm, SHM_SIZE);
        // closeSHM(data_shm, SHM_DATA_SIZE);
    }
}

void create_shm_sem()
{
    shmid_planes = openSHM(SHM_PLANES, SHM_SIZE);
    if (shmid_planes == -1)
    {
        perror("openSHM Cargo Plane Error");
        exit(1);
    }
    containers_shm = (int *)mapSHM(shmid_planes, SHM_SIZE);
    if (containers_shm == NULL)
    {
        perror("mapSHM Cargo Plane Error");
        exit(1);
    }

    shmid_data = openSHM(SHM_DATA, SHM_DATA_SIZE);
    if (shmid_data == -1)
    {
        perror("openSHM Shared Data Error");
        exit(1);
    }
    data_shm = (int *)mapSHM(shmid_data, SHM_DATA_SIZE);
    if (data_shm == NULL)
    {
        perror("mapSHM Shared Data Error");
        exit(1);
    }

    sem_containers = sem_open(SEM_CONTAINERS, O_CREAT | O_RDWR, 0666, 1);
    if (sem_containers == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    sem_data = sem_open(SEM_DATA, O_CREAT | O_RDWR, 0666, 1);
    if (sem_data == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    printf("Shared memory and semaphores created successfully\n");
}

void setupSignals()
{
    if (sigset(SIGUSR1, signalHandler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
    if (sigset(SIGUSR2, signalHandler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
}

// Function to read configuration.txt
void loadConfiguration(const char *filename, Config *config)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("Error opening the configuration file");
        exit(EXIT_FAILURE);
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file))
    {
        char key[50];
        int value;
        if (buffer[0] == '#' || buffer[0] == '\n')
            continue; // Ignore comments and empty lines
        if (sscanf(buffer, "%s = %d", key, &value) == 2)
        {
            if (strcmp(key, "NUM_CARGO_PLANES") == 0)
                config->numCargoPlanes = value;
            else if (strcmp(key, "MIN_CONTAINERS_PER_PLANE") == 0)
                config->minContainersPerPlane = value;
            else if (strcmp(key, "MAX_CONTAINERS_PER_PLANE") == 0)
                config->maxContainersPerPlane = value;
            else if (strcmp(key, "MIN_DROP_FREQUENCY") == 0)
                config->minDropFrequency = value;
            else if (strcmp(key, "MAX_DROP_FREQUENCY") == 0)
                config->maxDropFrequency = value;
            else if (strcmp(key, "MIN_REFILL_PERIOD") == 0)
                config->minRefillPeriod = value;
            else if (strcmp(key, "MAX_REFILL_PERIOD") == 0)
                config->maxRefillPeriod = value;
            else if (strcmp(key, "NUM_COLLECTING_COMMITTEES") == 0)
                config->numCollectingCommittees = value;
            else if (strcmp(key, "WORKERS_PER_COMMITTEE") == 0)
                config->workersPerCommittee = value;
            else if (strcmp(key, "MISSILE_HIT_PROBABILITY") == 0)
                config->missileHitProbability = value;
            else if (strcmp(key, "BAGS_PER_TRIP") == 0)
                config->bagsPerTrip = value;
            else if (strcmp(key, "STARVATION_RATE_THRESHOLD") == 0)
                config->starvationRateThreshold = value;
            else if (strcmp(key, "SIMULATION_DURATION_THRESHOLD") == 0)
                config->simulationDurationThreshold = value;
            else if (strcmp(key, "FAMILY_DEATH_RATE_THRESHOLD") == 0)
                config->familyDeathRateThreshold = value;
            else if (strcmp(key, "PLANE_CRASH_THRESHOLD") == 0)
                config->planeCrashThreshold = value;
            else if (strcmp(key, "CONTAINER_SHOT_DOWN_THRESHOLD") == 0)
                config->containerShotDownThreshold = value;
            else if (strcmp(key, "COLLECTING_WORKER_MARTYR_THRESHOLD") == 0)
                config->collectingWorkerMartyrThreshold = value;
            else if (strcmp(key, "DISTRIBUTING_WORKER_MARTYR_THRESHOLD") == 0)
                config->distributingWorkerMartyrThreshold = value;
            else if (strcmp(key, "FAMILY_STARVATION_DEATH_THRESHOLD") == 0)
                config->familyStarvationDeathThreshold = value;
        }
    }
    fclose(file);
}
