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

// Function Prototypes:
void loadConfiguration(const char *, Config *);
void initialize_cargo_plane();

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <configuration_file_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    loadConfiguration(argv[1], &config);

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
    kill(cargoPlanes[0], SIGUSR1); // send SIGUSR1 to the first cargo plane
    sleep(5);

    // open planes shared memory
    int shmid_planes = openSHM(SHM_PLANES, SHM_SIZE);
    if (shmid_planes == -1)
    {
        perror("openSHM Parent Error");
        exit(1);
    }

    // Map the shared memory segment to the address space of the process
    int *planes_shm = (int *)mapSHM(shmid_planes, SHM_SIZE);
    // read the containers from shared memory (One Container each time)
    printf("Reading from shared memory:\n");
    printf("Total Containers Dropped: %d\n", containersPerPlane[0]);
    for (int i = 0; i < containersPerPlane[0]; i++)
    {
        FlourContainer *container = (FlourContainer *)planes_shm;
        printf("Container ID: %d, Quantity: %d, Height: %d\n", container->container_id, container->quantity, container->height);
        planes_shm += 3;
    }
    // Wait for all cargo planes to finish
    for (int i = 0; i < config.numCargoPlanes; i++)
    {
        waitpid(cargoPlanes[i], NULL, 0);
    }
    return 0;
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
