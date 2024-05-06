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
    int FAMILY_DIED_THRESHOLD;
    int planeCrashThreshold;
    int containerShotDownThreshold;
    int collectingWorkerMartyrThreshold;
    int distributingWorkerMartyrThreshold;
    int familyStarvationDeathThreshold;
    int WORKER_MIN_ENERGY;
    int WORKER_MAX_ENERGY;
    int WORKER_ENERGY_PER_TRIP;
    int NUM_DITRIBUTING_COMMITTEES;
    int DISTRIBUTING_WORKERS_PER_COMMITTEE;
    int DISTRIBUTING_WORKER_CAPACITY;
    int NUM_SPLITTING_WORKERS;
    int NUM_OF_FAMILIES;
} Config;

// containers
int shmid_planes;
int *containers_shm;
sem_t *sem_containers;
// sahred data
int shmid_data;
int *data_shm;
sem_t *sem_data;
// safe area
int safe_shmid;
int *safe_shm_ptr;
sem_t *sem_safe;
// families
int families_shmid;
int *families_shm;
sem_t *sem_families;
// stage 2 shared data
int shmid_stage2;
int *stage2_shm;
sem_t *sem_stage2;

// Function Prototypes:
void loadConfiguration(const char *, Config *);
void initialize_cargo_plane();
void initialize_shared_data();
void setupSignals();
void signalHandler(int sig);
void create_shm_sem();
void close_all();
void test();
void checkTerminationConditions();
void killConfirmed();

Config config;
pid_t *cargoPlanes;
pid_t monitoringProcess;
pid_t *collectingCommittees;
int *containersPerPlane;
pid_t *splitter;
pid_t *distributers;
pid_t familyProcess;
pid_t occupationForces;
// union sigval value;
int signal_value;
int numOfCollectorsKilled = 0;
int numOfDistributersKilled = 0;
int numOfSplittersMissed = 0;
int victimCommittee;

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <configuration_file_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    srand(time(NULL) ^ (getpid() << 16)); // Seed random generator based on planeID
    loadConfiguration(argv[1], &config);
    // dynamically allocate memory for cargo planes and collecting committees
    cargoPlanes = (pid_t *)malloc(config.numCargoPlanes * sizeof(pid_t));
    collectingCommittees = (pid_t *)malloc(config.numCollectingCommittees * sizeof(pid_t));
    containersPerPlane = (int *)malloc(config.numCargoPlanes * sizeof(int));
    distributers = (pid_t *)malloc(config.NUM_DITRIBUTING_COMMITTEES * sizeof(pid_t));
    splitter = (pid_t *)malloc(config.NUM_SPLITTING_WORKERS * sizeof(pid_t));

    setupSignals();
    create_shm_sem();
    initialize_shared_data();

    pid_t pid;
    char arg1[10], arg2[10], arg3[10], arg4[10], arg5[10], arg6[10], arg7[10];

    /* ===================================================== */
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
        }
    }
    sleep(2);
    for (int i = 0; i < config.numCargoPlanes; i++)
    {
        kill(cargoPlanes[i], SIGUSR1);
    }
    /* ===================================================== */
    monitoringProcess = fork();
    if (monitoringProcess == -1)
    {
        perror("fork");
        exit(1);
    }
    if (monitoringProcess == 0)
    {
        int containersThreshold = config.containerShotDownThreshold;
        sprintf(arg1, "%d", containersThreshold);
        execl("./monitor", "monitor", arg1, NULL);
        perror("execl");
        exit(1);
    }
    sleep(2);
    kill(monitoringProcess, SIGALRM); // Start monitoring
    /* create collecting committees ===================================================== */
    for (int i = 0; i < config.numCollectingCommittees; i++)
    {
        pid = fork();
        if (pid == -1)
        {
            perror("fork");
            exit(1);
        }
        if (pid == 0)
        {
            /* open file to redirect stdout  filename = "collecting_%d.log" */
            char filename[50];
            sprintf(filename, "collecting_%d.log", i);
            int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (file == -1)
            {
                perror("open");
                exit(EXIT_FAILURE);
            }
            // redirect the standard output to the file
            int fileDescriptor = dup2(file, STDOUT_FILENO);
            if (fileDescriptor == -1)
            {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(file);
            int committee_id = i;
            int committee_size = config.workersPerCommittee;
            int min_energy = config.WORKER_MIN_ENERGY;
            int max_energy = config.WORKER_MAX_ENERGY;
            int energy_per_trip = config.WORKER_ENERGY_PER_TRIP;
            sprintf(arg1, "%d", committee_id);
            sprintf(arg2, "%d", committee_size);
            sprintf(arg3, "%d", min_energy);
            sprintf(arg4, "%d", max_energy);
            sprintf(arg5, "%d", energy_per_trip);
            execl("./collectors", "collectors", arg1, arg2, arg3, arg4, arg5, NULL);
            perror("execl");
            exit(1);
        }
        else if (pid > 0)
        {
            collectingCommittees[i] = pid;
        }
    }
    sleep(1);
    // create splitters
    for (int i = 0; i < config.NUM_SPLITTING_WORKERS; i++)
    {
        pid = fork();
        if (pid == -1)
        {
            perror("fork");
            exit(1);
        }
        if (pid == 0)
        {
            sprintf(arg1, "%d", i);
            execl("./splitter", "splitter", arg1, NULL);
            perror("execl");
            exit(1);
        }
        else if (pid > 0)
        {
            splitter[i] = pid;
        }
    }
    sleep(1);
    // create distribution commitees
    for (int i = 0; i < config.NUM_DITRIBUTING_COMMITTEES; i++)
    {
        pid = fork();
        if (pid == -1)
        {
            perror("fork");
            exit(1);
        }
        if (pid == 0)
        {
            /* open file to redirect stdout  filename = "distribution_%d.txt" */
            char filename[50];
            sprintf(filename, "distribution_%d.log", i);
            int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (file == -1)
            {
                perror("open");
                exit(EXIT_FAILURE);
            }
            // redirect the standard output to the file
            int fileDescriptor = dup2(file, STDOUT_FILENO);
            if (fileDescriptor == -1)
            {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(file);

            int committee_id = i;
            int committee_size = config.DISTRIBUTING_WORKERS_PER_COMMITTEE;
            int min_energy = config.WORKER_MIN_ENERGY;
            int max_energy = config.WORKER_MAX_ENERGY;
            int energy_per_trip = config.WORKER_ENERGY_PER_TRIP;
            int capacity = config.DISTRIBUTING_WORKER_CAPACITY;
            sprintf(arg1, "%d", committee_id);
            sprintf(arg2, "%d", committee_size);
            sprintf(arg3, "%d", min_energy);
            sprintf(arg4, "%d", max_energy);
            sprintf(arg5, "%d", energy_per_trip);
            sprintf(arg6, "%d", capacity);
            execl("./distributers", "distributers", arg1, arg2, arg3, arg4, arg5, arg6, NULL);
            perror("execl");
            exit(1);
        }
        else if (pid > 0)
        {
            distributers[i] = pid;
        }
    }
    // create families process
    familyProcess = fork();
    if (familyProcess == -1)
    {
        perror("fork");
        exit(1);
    }
    if (familyProcess == 0)
    {
        // redirect the output into a file families.log
        int file = open("families.log", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (file == -1)
        {
            perror("open");
            exit(EXIT_FAILURE);
        }
        // redirect the standard output to the file
        int fileDescriptor = dup2(file, STDOUT_FILENO);
        if (fileDescriptor == -1)
        {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        close(file);
        // prepare args
        sprintf(arg1, "%d", config.NUM_OF_FAMILIES);
        sprintf(arg2, "%d", config.familyStarvationDeathThreshold);
        sprintf(arg3, "%d", config.FAMILY_DIED_THRESHOLD);
        execl("./families", "families", arg1, arg2, arg3, NULL);
        perror("execl");
        exit(1);
    }
    sleep(1);
    // create the occpuation forces process
    occupationForces = fork();
    if (occupationForces == -1)
    {
        perror("fork");
        exit(1);
    }
    if (occupationForces == 0)
    {
        // redirect the output into a file occupationForces.log
        int file = open("occupationForces.log", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (file == -1)
        {
            perror("open");
            exit(EXIT_FAILURE);
        }
        // redirect the standard output to the file
        int fileDescriptor = dup2(file, STDOUT_FILENO);
        if (fileDescriptor == -1)
        {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        close(file);
        execl("./occupation", "occupation", NULL);
        perror("execl");
        exit(1);
    }
    sleep(2);
    for (int i = 0; i < config.numCollectingCommittees; i++)
    {
        kill(collectingCommittees[i], SIGALRM); // Start collecting
    }
    // Wait for all cargo planes to finish
    sleep(2);
    // start splitter processes.
    for (int i = 0; i < config.NUM_SPLITTING_WORKERS; i++)
    {
        kill(splitter[i], SIGALRM);
    }
    sleep(2);
    // start distribution processes
    for (int i = 0; i < config.NUM_DITRIBUTING_COMMITTEES; i++)
    {
        kill(distributers[i], SIGALRM);
    }
    // redirect the output into a file parent.log
    int file = open("parent.log", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }
    // redirect the standard output to the file
    int fileDescriptor = dup2(file, STDOUT_FILENO);
    if (fileDescriptor == -1)
    {
        perror("dup2");
        exit(EXIT_FAILURE);
    }
    close(file);
    sleep(4);
    // start families process
    kill(familyProcess, SIGALRM);
    sleep(1);
    // start occupation forces process
    kill(occupationForces, SIGALRM);
    sleep(1);
    alarm(config.simulationDurationThreshold);
    while (1)
    {
        pause();
    }
    for (int i = 0; i < config.numCargoPlanes; i++)
    {
        waitpid(cargoPlanes[i], NULL, 0);
    }
    return 0;
}

void checkTerminationConditions()
{
    if (numOfCollectorsKilled >= config.collectingWorkerMartyrThreshold)
    {
        printf("\033[0;31mNumber of collecting workers killed %d, exceeded the threshold, Simulation will be terminated\n\033[0m", numOfCollectorsKilled);
        test();
    }
    if (numOfDistributersKilled >= config.distributingWorkerMartyrThreshold)
    {
        printf("\033[0;31mNumber of distributing workers killed %d, exceeded the threshold, Simulation will be terminated\n\033[0m", numOfDistributersKilled);
        test();
    }
}

void signalHandler(int sig)
{
    if (sig == SIGUSR1)
    {
        // choose a random value to determine which worker type to kill
        signal_value = rand() % 2 == 0 ? 1 : 2;
        if (signal_value == 1)
        {
            // try to kill a worker of the collecting committee
            victimCommittee = rand() % config.numCollectingCommittees;
            kill(collectingCommittees[victimCommittee], SIGUSR1);
        }
        else if (signal_value == 2)
        {
            // try to kill a worker of the distributing committee
            victimCommittee = rand() % config.NUM_DITRIBUTING_COMMITTEES;
            kill(distributers[victimCommittee], SIGUSR1);
        }
    }
    else if (sig == SIGUSR2)
    {
        killConfirmed(); // unfornately the worker has been killed
    }
    else if (sig == SIGINT)
    {
        // number of containers have been shot down exceeded the threshold
        printf("\033[0;31mNumber of containers shot down exceeded the threshold, Simulation will be terminated\n\033[0m");
        test();
    }
    else if (sig == SIGALRM)
    {
        printf("The app has been running for %d seconds, Simulation will be terminated\n", config.simulationDurationThreshold);
        test();
    }
    else if (sig == SIGTERM)
    {
        // number of families died exceeded the threshold
        printf("\033[0;31mNumber of families died exceeded the threshold, Simulation will be terminated\n\033[0m");
        test();
    }
}

void killConfirmed()
{
    // keep track of the number of killed workers
    if (signal_value == 1)
    {
        numOfCollectorsKilled++;
        printf("\033[0;31mNumber of collecting workers killed %d\n\033[0m", numOfCollectorsKilled);
    }
    else if (signal_value == 2)
    {
        numOfDistributersKilled++;
        printf("\033[0;31mNumber of distributing workers killed %d\n\033[0m", numOfDistributersKilled);
    }
    checkTerminationConditions();
    if (numOfSplittersMissed == config.NUM_SPLITTING_WORKERS)
    {
        printf("\033[0;31mAll splitters have been missed, No replacement will be done\n\033[0m");
    }
    else
    {
        // send a signal to the splitter process to make some splitter replace the killed worker
        int splitter_id = rand() % config.NUM_SPLITTING_WORKERS;
        // check if this splitter is alive and not missed in previous replacing process
        while (splitter[splitter_id] == -1)
        {
            splitter_id = rand() % config.NUM_SPLITTING_WORKERS;
        }
        kill(splitter[splitter_id], SIGUSR1); // notify the splitter to replace a worker
        numOfSplittersMissed++;               // keep track of the number of missed splitters
        splitter[splitter_id] = -1;           // mark this splitter as missed
        // now the parent should send a signal to notify the committee to replace the killed worker
        if (signal_value == 1)
        {
            // notify the collecting committee that there is a new worker will join the committee
            kill(collectingCommittees[victimCommittee], SIGUSR2);
        }
        else if (signal_value == 2)
        {
            // notify the distributing committee that there is a new worker will join the committee
            kill(distributers[victimCommittee], SIGUSR2);
        }
    }
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
    if (sigset(SIGINT, signalHandler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
    if (sigset(SIGALRM, signalHandler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
    if (sigset(SIGTERM, signalHandler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
}

void test()
{
    // send TSTP signal to the Cargo Planes
    for (int i = 0; i < config.numCargoPlanes; i++)
    {
        printf("Parent sent SIGTSTP to plane %d with PID %d\n", i, cargoPlanes[i]);
        kill(cargoPlanes[i], SIGTSTP);
    }
    // send TSTP signal to the monitoring process
    kill(monitoringProcess, SIGTSTP);
    // send TSTP signal to all collecting committees
    for (int i = 0; i < config.numCollectingCommittees; i++)
    {
        printf("Parent sent SIGTSTP to committee %d with PID %d\n", i, collectingCommittees[i]);
        kill(collectingCommittees[i], SIGTSTP);
    }
    sleep(1);
    // send TSTP signal to all splitter processes
    for (int i = 0; i < config.NUM_SPLITTING_WORKERS; i++)
    {
        if (splitter[i] != -1)
        {
            printf("Parent sent SIGTSTP to splitter %d with PID %d\n", i, splitter[i]);
            kill(splitter[i], SIGTSTP);
        }
    }
    sleep(1);
    // send TSTP signal to all distributers
    for (int i = 0; i < config.NUM_DITRIBUTING_COMMITTEES; i++)
    {
        printf("Parent sent SIGTSTP to distributer %d with PID %d\n", i, distributers[i]);
        kill(distributers[i], SIGTSTP);
    }
    // send TSTP signal to families process
    printf("Parent sent SIGTSTP to families process with PID %d\n", familyProcess);
    kill(familyProcess, SIGTSTP);

    // if (sem_wait(sem_data) == -1)
    // {
    //     perror("sem_wait");
    //     exit(1);
    // }
    int totalContainersDropped = ((SharedData *)data_shm)->totalContainersDropped;
    int totalLandedContainers = ((SharedData *)data_shm)->totalLandedContainers;
    int totalCrashedContainers = ((SharedData *)data_shm)->numOfCrashedContainers;
    int collectedContainers = ((SharedData *)data_shm)->cleectedContainers;
    printf("Parent Entered data critical section\n");
    printf("------------------------------------------\n");
    // if (sem_wait(sem_containers) == -1)
    // {
    //     perror("sem_wait");
    //     exit(1);
    // }
    // if (sem_wait(sem_safe) == -1)
    // {
    //     perror("sem_wait");
    //     exit(1);
    // }
    printf("Parent entered containers critical section\n");
    printf("==========================================\n");
    int summation = 0;
    for (int i = 0; i < config.numCargoPlanes; i++)
    {
        summation += containersPerPlane[i];
    }
    summation *= 3;
    int *temp = containers_shm;
    printf("Total Containers %d\n", summation);
    printf("Containers dropped: %d\n", totalContainersDropped);
    printf("Landed containers: %d\n", totalLandedContainers);
    printf("Crashed containers: %d\n", totalCrashedContainers);
    printf("Collected containers: %d\n", collectedContainers);
    // read containers of the first cargo plane from shared memory
    printf("\033[0;33mNumber of Containers in the Air = %d\n\033[0m", (totalContainersDropped - totalLandedContainers - totalCrashedContainers));
    int index = 0;
    while ((index < totalContainersDropped))
    {
        FlourContainer *container = (FlourContainer *)temp;
        if (container->crahshed || container->landed || container->collected)
        {
            // printf("Container %d is not in the Air\n", index);
            temp += sizeof(FlourContainer);
            index++;
            continue;
        }
        printf("\033[0;32mContainer %d: | Quantity=  %d | height = %d | Collected = %d | Landed = %d | Crahsed = %d | \n\033[0m",
               index, container->quantity, container->height, container->collected, container->landed, container->crahshed);
        index++;
        temp += sizeof(FlourContainer);
    }
    printf("/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|\n");
    printf("\033[0;33m|| Number of Containers in the Landed Area = %d ||\n\033[0m", totalLandedContainers - collectedContainers);
    temp = containers_shm;
    index = 0;
    while (index < totalContainersDropped)
    {
        FlourContainer *container = (FlourContainer *)temp;
        if (container->landed && !container->collected && !container->crahshed)
        {
            printf("\033[0;32mContainer %d: | Quantity=  %d | height = %d | Collected = %d | Landed = %d | Crahsed = %d | \n\033[0m",
                   index, container->quantity, container->height, container->collected, container->landed, container->crahshed);
        }
        temp += sizeof(FlourContainer);
        index++;
    }
    printf("/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|\n");
    int collectedBags = 0;
    printf("\033[0;33m|| Number of Containers in the Safe Area = %d ||\n\033[0m", collectedContainers);
    temp = safe_shm_ptr;
    index = 0;
    while (index < collectedContainers)
    {
        FlourContainer *container = (FlourContainer *)temp;
        if (container->collected)
        {
            printf("\033[0;32mContainer %d: | Quantity=  %d | height = %d | Collected = %d | Landed = %d | Crahsed = %d | \n\033[0m",
                   index, container->quantity, container->height, container->collected, container->landed, container->crahshed);
            collectedBags += container->quantity;
        }
        temp += sizeof(FlourContainer);
        index++;
    }
    printf("\033[0;33m|| Number of Bags in the Safe Area = %d ||\n\033[0m", collectedBags);
    // update shared data with the new values
    printf("/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|/|\n");
    ((SharedData *)data_shm)->totalContainersDropped = totalContainersDropped;
    // print number of splitted containers and the total weight splitted
    // if (sem_wait(sem_stage2) == -1)
    // {
    //     perror("sem_wait");
    //     exit(1);
    // }
    STAGE2_DATA *stage2 = (STAGE2_DATA *)stage2_shm;
    printf("\033[0;33m|| Number of Containers have been splitted = %d ||\n\033[0m", stage2->numOfSplittedContainers);
    printf("\033[0;33m|| Number of available Bags = %d ||\n\033[0m", stage2->numOfBags);
    printf("\033[0;33m|| Number of distributed Bags = %d ||\n\033[0m", stage2->distributedBags);
    printf("\033[0;33m|| Number of killed collecting workers = %d ||\n\033[0m", numOfCollectorsKilled);
    printf("\033[0;33m|| Number of killed distributing workers = %d ||\n\033[0m", numOfDistributersKilled);
    printf("\033[0;33m|| Number of missed splitters = %d ||\n\033[0m", numOfSplittersMissed);
    // if (sem_post(sem_stage2) == -1)
    // {
    //     perror("sem_post");
    //     exit(1);
    // }
    printf("\033[0;33mAfter collection process:\n\033[0m", totalContainersDropped);
    printf("Total containers dropped: %d\n Total landed containers: %d\n Total crashed containers: %d\n Total collected containers: %d Total splitted containers: %d\n",
           totalContainersDropped, totalLandedContainers, totalCrashedContainers, collectedContainers, stage2->numOfSplittedContainers);
    // if (sem_post(sem_containers) == -1)
    // {
    //     perror("sem_post");
    //     exit(1);
    // }
    // if (sem_post(sem_data) == -1)
    // {
    //     perror("sem_post");
    //     exit(1);
    // }
    // if (sem_post(sem_safe) == -1)
    // {
    //     perror("sem_post");
    //     exit(1);
    // }
    printf("Parent exited the critical section\n");
    // kill all child processes
    for (int i = 0; i < config.numCargoPlanes; i++)
    {
        kill(cargoPlanes[i], SIGKILL);
    }
    for (int i = 0; i < config.numCollectingCommittees; i++)
    {
        kill(collectingCommittees[i], SIGKILL);
    }
    kill(monitoringProcess, SIGKILL);
    // remove all shared memory and semaphores
    close_all();
    printf("All child processes killed, parent process will exit now\n");
    exit(0);
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
    data->totalLandedContainers = 0;
    data->numOfCrashedContainers = 0;
    data->numOfSplittedContainers = 0;
    data->numOfDistributedContainers = 0;
    data->numOfBags = 0;
    if (sem_post(sem_data) == -1)
    {
        perror("sem_post");
        exit(1);
    }
    if (sem_wait(sem_stage2) == -1)
    {
        perror("sem_wait");
        exit(1);
    }
    STAGE2_DATA *stage2 = (STAGE2_DATA *)stage2_shm;
    stage2->numOfSplittedContainers = 0;
    stage2->numOfBags = 0;
    stage2->numOfDistributedContainers = 0;
    stage2->numOFCollectedContainers = 0;
    stage2->distributedBags = 0;
    stage2->NUM_OF_FAMILIES = config.NUM_OF_FAMILIES;
    if (sem_post(sem_stage2) == -1)
    {
        perror("sem_post");
        exit(1);
    }
    printf("Shared data initialized successfully\n");
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
            else if (strcmp(key, "SIMULATION_DURATION_THRESHOLD") == 0)
                config->simulationDurationThreshold = value;
            else if (strcmp(key, "FAMILY_DIED_THRESHOLD") == 0)
                config->FAMILY_DIED_THRESHOLD = value;
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
            else if (strcmp(key, "WORKER_MIN_ENERGY") == 0)
                config->WORKER_MIN_ENERGY = value;
            else if (strcmp(key, "WORKER_MAX_ENERGY") == 0)
                config->WORKER_MAX_ENERGY = value;
            else if (strcmp(key, "WORKER_ENERGY_PER_TRIP") == 0)
                config->WORKER_ENERGY_PER_TRIP = value;
            else if (strcmp(key, "NUM_DITRIBUTING_COMMITTEES") == 0)
                config->NUM_DITRIBUTING_COMMITTEES = value;
            else if (strcmp(key, "DISTRIBUTING_WORKERS_PER_COMMITTEE") == 0)
                config->DISTRIBUTING_WORKERS_PER_COMMITTEE = value;
            else if (strcmp(key, "DISTRIBUTING_WORKER_CAPACITY") == 0)
                config->DISTRIBUTING_WORKER_CAPACITY = value;
            else if (strcmp(key, "NUM_SPLITTING_WORKERS") == 0)
                config->NUM_SPLITTING_WORKERS = value;
            else if (strcmp(key, "NUM_OF_FAMILIES") == 0)
                config->NUM_OF_FAMILIES = value;
        }
    }
    fclose(file);
}

void create_shm_sem()
{
    // Containers Shared Memory
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
    // Data Shared Memory
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
    // Safe Area Shared Memory
    safe_shmid = openSHM(SHM_SAFE, SHM_SAFE_SIZE);
    if (safe_shmid == -1)
    {
        perror("openSHM Safe Error");
        exit(1);
    }
    safe_shm_ptr = (int *)mapSHM(safe_shmid, SHM_SAFE_SIZE);
    if (safe_shm_ptr == NULL)
    {
        perror("mapSHM Safe Error");
        exit(1);
    }
    // Families Shared Memory
    families_shmid = openSHM(FAMILIES_SHM, FAMILIES_SHM_SIZE);
    if (families_shmid == -1)
    {
        perror("openSHM Families Error");
        exit(1);
    }
    families_shm = (int *)mapSHM(families_shmid, FAMILIES_SHM_SIZE);
    if (families_shm == NULL)
    {
        perror("mapSHM Families Error");
        exit(1);
    }
    // Stage 2 Data Shared Memory
    shmid_stage2 = openSHM(SHM_STAGE2, SHM_STAGE2_SIZE);
    if (shmid_stage2 == -1)
    {
        perror("openSHM Stage 2 Error");
        exit(1);
    }
    stage2_shm = (int *)mapSHM(shmid_stage2, SHM_STAGE2_SIZE);
    if (stage2_shm == NULL)
    {
        perror("mapSHM Stage 2 Error");
        exit(1);
    }
    // Semaphores
    sem_containers = sem_open(SEM_CONTAINERS, O_CREAT | O_EXCL | O_RDWR, 0666, 1);
    if (sem_containers == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    sem_data = sem_open(SEM_DATA, O_CREAT | O_EXCL | O_RDWR, 0666, 1);
    if (sem_data == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    sem_safe = sem_open(SEM_SAFE, O_CREAT | O_EXCL | O_RDWR, 0666, 1);
    if (sem_safe == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    sem_families = sem_open(SEM_FAMILIES, O_CREAT | O_EXCL | O_RDWR, 0666, 1);
    if (sem_families == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    sem_stage2 = sem_open(SEM_STAGE2, O_CREAT | O_EXCL | O_RDWR, 0666, 1);
    if (sem_stage2 == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    int sem_value;
    if (sem_getvalue(sem_containers, &sem_value) == -1)
    {
        perror("sem_getvalue");
        exit(1);
    }
    printf("Continaers Semaphore value: %d\n", sem_value);
    if (sem_getvalue(sem_data, &sem_value) == -1)
    {
        perror("sem_getvalue");
        exit(1);
    }
    printf("Data Semaphore value: %d\n", sem_value);
    if (sem_getvalue(sem_safe, &sem_value) == -1)
    {
        perror("sem_getvalue");
        exit(1);
    }
    printf("Landed Containers Semaphore value: %d\n", sem_value);
    if (sem_getvalue(sem_stage2, &sem_value) == -1)
    {
        perror("sem_getvalue");
        exit(1);
    }
    printf("Stage 2 Semaphore value: %d\n", sem_value);
    if (sem_getvalue(sem_families, &sem_value) == -1)
    {
        perror("sem_getvalue");
        exit(1);
    }
    printf("Families Semaphore value: %d\n", sem_value);
    printf("Shared memory and semaphores created successfully\n");
}

void close_all()
{
    // remove semaphores
    if (sem_close(sem_containers) == -1)
    {
        perror("sem_close");
        exit(1);
    }
    if (sem_close(sem_data) == -1)
    {
        perror("sem_close");
        exit(1);
    }
    if (sem_close(sem_safe) == -1)
    {
        perror("sem_close");
        exit(1);
    }
    if (sem_close(sem_stage2) == -1)
    {
        perror("sem_close");
        exit(1);
    }
    if (sem_close(sem_families) == -1)
    {
        perror("sem_close");
        exit(1);
    }
    if (sem_unlink(SEM_CONTAINERS) == -1)
    {
        perror("sem_unlink");
        exit(1);
    }
    if (sem_unlink(SEM_DATA) == -1)
    {
        perror("sem_unlink");
        exit(1);
    }
    if (sem_unlink(SEM_SAFE) == -1)
    {
        perror("sem_unlink");
        exit(1);
    }
    if (sem_unlink(SEM_STAGE2) == -1)
    {
        perror("sem_unlink");
        exit(1);
    }
    if (sem_unlink(SEM_FAMILIES) == -1)
    {
        perror("sem_unlink");
        exit(1);
    }
    // remove shared memory
    if (shm_unlink(SHM_PLANES) == -1)
    {
        perror("shm_unlink");
        exit(1);
    }
    if (shm_unlink(SHM_DATA) == -1)
    {
        perror("shm_unlink");
        exit(1);
    }
    if (shm_unlink(SHM_SAFE) == -1)
    {
        perror("shm_unlink");
        exit(1);
    }
    if (shm_unlink(SHM_STAGE2) == -1)
    {
        perror("shm_unlink");
        exit(1);
    }
    if (shm_unlink(FAMILIES_SHM) == -1)
    {
        perror("shm_unlink");
        exit(1);
    }
    printf("All shared memory and semaphores removed successfully\n");

    // free allocated memory
    free(cargoPlanes);
    free(collectingCommittees);
    free(containersPerPlane);
    free(distributers);
    free(splitter);
    printf("All allocated memory freed successfully\n");
}
