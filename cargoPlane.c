#include "local.h"
#include "functions.c"

void initialize_cargo_plane();
void print_cargo_plane(cargoPlane plane);
void signalHandler(int sig);
void signalSetup();
void dropContainers();
void refillContainers();
void open_shm_sem();

cargoPlane current_cargoPlane;
int planeID, numOfContainers, minDropFreq, maxDropFreq, minRefill, maxRefill;
int cont_shmid;
int *cont_shm_ptr;
int data_shmid;
int *data_shm_ptr;
sem_t *sem_containers;
sem_t *sem_data;
int remainingContainers;
int elementIndex = 0;

int CRITICAL = 0;

int main(int argc, char *argv[])
{
    if (argc != 7)
    {
        fprintf(stderr, "Usage: %s <planeID> <numOfContainers> <minDropFreq> <maxDropFreq> <minRefill> <maxRefill>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse the configuration parameters
    planeID = atoi(argv[1]);
    numOfContainers = atoi(argv[2]);
    minDropFreq = atoi(argv[3]);
    maxDropFreq = atoi(argv[4]);
    minRefill = atoi(argv[5]);
    maxRefill = atoi(argv[6]);
    remainingContainers = numOfContainers;

    // Initialize the cargo plane
    signalSetup();
    initialize_cargo_plane();
    open_shm_sem();

    while (1)
    {
        pause();
    }
    printf("Cargo plane %d has been killed\n", current_cargoPlane.plane_id);

    return 0;
}

void signalHandler(int sig)
{
    if (sig == SIGUSR1)
    {
        // printf("Cargo Plane %d received SIGUSR1\n", current_cargoPlane.plane_id);
        alarm(current_cargoPlane.dropFrequency);
    }
    else if (sig == SIGALRM)
    {
        if (remainingContainers == 0)
        {
            // refill the containers
            int refillTime = rand() % (maxRefill - minRefill + 1) + minRefill;
            printf("\033[0;31mCargo Plane %d has dropped all containers and will refill in %d seconds\033[0m\n",
                   current_cargoPlane.plane_id, refillTime);
            refillContainers();
            alarm(current_cargoPlane.dropFrequency);
        }
        else
        {
            dropContainers();
            alarm(current_cargoPlane.dropFrequency); // wait for the next drop
        }
    }
    else if (sig == SIGTSTP)
    {
        printf("Cargo Plane %d received SIGTSTP\n", current_cargoPlane.plane_id);
        printf("\033[0;31mExiting cargo plane %d\n\033[0m", current_cargoPlane.plane_id);
        if (CRITICAL == 1)
        {
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
        }
        exit(0);
    }
    else
    {
        printf("Cargo Plane %d received an unknown signal\n", current_cargoPlane.plane_id);
    }
}
void refillContainers()
{
    current_cargoPlane.status = 2;
    remainingContainers = numOfContainers;
    current_cargoPlane.y_axis = rand() % (120 - 80 + 1) + 100;
    // initialize the arrays of containers
    for (int i = 0; i < current_cargoPlane.numContainers; i++)
    {
        current_cargoPlane.containers[i].quantity = rand() % (100 - 15 + 1) + 15;
        current_cargoPlane.containers[i].height = current_cargoPlane.y_axis;
    }
}

void dropContainers()
{
    current_cargoPlane.status = 1;
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
    /* Drop the container */
    int totalContainersDropped = ((SharedData *)data_shm_ptr)->totalContainersDropped;
    int *temp_ptr = cont_shm_ptr;
    temp_ptr += sizeof(FlourContainer) * totalContainersDropped;
    memcpy(temp_ptr, &current_cargoPlane.containers[numOfContainers - remainingContainers], sizeof(FlourContainer));
    remainingContainers--;
    ((SharedData *)data_shm_ptr)->totalContainersDropped++;
    printf("\033[0;32mCargo Plane %d has dropped a container\033[0m\n", current_cargoPlane.plane_id);
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
}

void open_shm_sem()
{
    cont_shmid = openSHM(SHM_PLANES, SHM_SIZE);
    if (cont_shmid == -1)
    {
        perror("openSHM Cargo Plane Error");
        exit(1);
    }
    cont_shm_ptr = (int *)mapSHM(cont_shmid, SHM_SIZE);
    if (cont_shm_ptr == NULL)
    {
        perror("mapSHM Cargo Plane Error");
        exit(1);
    }

    data_shmid = openSHM(SHM_DATA, SHM_DATA_SIZE);
    if (data_shmid == -1)
    {
        perror("openSHM Shared Data Error");
        exit(1);
    }
    data_shm_ptr = (int *)mapSHM(data_shmid, SHM_DATA_SIZE);
    if (data_shm_ptr == NULL)
    {
        perror("mapSHM Shared Data Error");
        exit(1);
    }

    sem_containers = sem_open(SEM_CONTAINERS, O_RDWR);
    if (sem_containers == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }

    sem_data = sem_open(SEM_DATA, O_RDWR);
    if (sem_data == SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }
}

void initialize_cargo_plane()
{
    srand(time(NULL) ^ (getpid() << 16)); // Seed random generator based on planeID
    current_cargoPlane.plane_pid = getpid();
    current_cargoPlane.plane_id = planeID;
    current_cargoPlane.numContainers = numOfContainers;
    current_cargoPlane.status = 0;
    current_cargoPlane.dropFrequency = rand() % (maxDropFreq - minDropFreq + 1) + minDropFreq;
    current_cargoPlane.valid = 1;
    current_cargoPlane.y_axis = rand() % (120 - 80 + 1) + 80;
    current_cargoPlane.x_axis = rand() % (100 - 10 + 1) + 10;
    // initialize the arrays of containers
    for (int i = 0; i < current_cargoPlane.numContainers; i++)
    {
        FlourContainer container;
        // container.container_id = i + 1;
        container.quantity = rand() % (100 - 15 + 1) + 15;
        container.height = current_cargoPlane.y_axis;
        container.collected = 0; /* 0 => not collected, 1 => collected */
        container.landed = 0;    /* 0 => not landed, 1 => landed */
        container.crahshed = 0;  /* 0 => not crashed, 1 => crashed */
        // container.splitted = 0;  /* 0 => not splitted, 1 => splitted */
        current_cargoPlane.containers[i] = container;
    }
}

void print_cargo_plane(cargoPlane plane)
{
    printf("Plane ID: %d\n", plane.plane_id);
    printf("Number of containers: %d\n", plane.numContainers);
    printf("Drop frequency: %d\n", plane.dropFrequency);
    printf("Status: %d\n", plane.status);
    printf("Valid: %d\n", plane.valid);
    printf("Y-axis: %d\n", plane.y_axis);
    printf("X-axis: %d\n", plane.x_axis);
    printf("Containers:\n");
    for (int i = 0; i < plane.numContainers; i++)
    {
        printf("Quantity: %d, Height: %d\n", plane.containers[i].quantity, plane.containers[i].height);
    }
    exit(1);
}

void signalSetup()
{
    if (sigset(SIGUSR1, signalHandler) == SIG_ERR)
    {
        perror("sigset");
        exit(1);
    }

    if (sigset(SIGALRM, signalHandler) == SIG_ERR)
    {
        perror("sigset");
        exit(1);
    }
    if (sigset(SIGTSTP, signalHandler) == SIG_ERR)
    {
        perror("sigset");
        exit(1);
    }
}
