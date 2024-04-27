#include "local.h"
#include "functions.c"

void initialize_cargo_plane();
void print_cargo_plane(cargoPlane plane);
void signalHandler(int sig);
void signalSetup();
// void writeSHM();
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
    printf("Droping frequency: %d\n", current_cargoPlane.dropFrequency);

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
        // printf("Cargo Plane %d received SIGALRM\n", current_cargoPlane.plane_id);
        if (remainingContainers == 0)
        {
            printf("\033[0;32mCargo Plane %d has dropped all containers\033[0m\n", current_cargoPlane.plane_id);
            // open data shared memory
            if (sem_wait(sem_data) == -1)
            {
                perror("waitSEM");
                exit(1);
            }
            int totalPlanes = ((SharedData *)data_shm_ptr)->numOfCargoPlanes;
            int planesDropped = ((SharedData *)data_shm_ptr)->planesDropped;
            planesDropped++;
            ((SharedData *)data_shm_ptr)->planesDropped = planesDropped;
            if (planesDropped == totalPlanes)
            {
                // all planes have dropped their containers
                printf("\033[0;31mAll planes have dropped their containers\033[0m\n");
                kill(getppid(), SIGUSR1);
            }
            if (sem_post(sem_data) == -1)
            {
                perror("signalSEM");
                exit(1);
            }
        }
        else
        {
            current_cargoPlane.status = 1;
            dropContainers();
            alarm(current_cargoPlane.dropFrequency); // wait for the next drop
        }
    }
}

void dropContainers()
{
    printf("Cargo Plane %d dropping a container\n", current_cargoPlane.plane_id);
    if (sem_wait(sem_data) == -1)
    {
        perror("waitSEM");
        exit(1);
    }
    int totalContainersDropped = ((SharedData *)data_shm_ptr)->totalContainersDropped;
    if (sem_wait(sem_containers) == -1)
    {
        perror("waitSEM");
        exit(1);
    }
    printf("Cargo Plane %d locked SEM\n", current_cargoPlane.plane_id);
    // SharedData *sharedData = ;
    int *temp_ptr = cont_shm_ptr;
    elementIndex = 0;
    while ((((FlourContainer *)temp_ptr)->height != -1) && (totalContainersDropped > elementIndex))
    {
        elementIndex++;
        temp_ptr += sizeof(FlourContainer);
    }
    // printf("Cargo Plane %d Opened SHM\n", current_cargoPlane.plane_id);
    printf("Writing container %d to at address %p\n", current_cargoPlane.containers[numOfContainers - remainingContainers].container_id, temp_ptr);
    // printf("Total containers dropped: %d\n", totalContainersDropped);
    memcpy(temp_ptr, &current_cargoPlane.containers[numOfContainers - remainingContainers], sizeof(FlourContainer));
    remainingContainers--;

    ((SharedData *)data_shm_ptr)->totalContainersDropped = totalContainersDropped + 1;
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
    printf("Cargo Plane %d unlocked SEM's\n", current_cargoPlane.plane_id);
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
    current_cargoPlane.y_axis = rand() % (120 - 100 + 1) + 100;
    current_cargoPlane.x_axis = rand() % (100 - 10 + 1) + 10;
    // initialize the arrays of containers
    for (int i = 0; i < current_cargoPlane.numContainers; i++)
    {
        FlourContainer container;
        container.container_id = i + 1;
        container.quantity = rand() % (100 - 15 + 1) + 15;
        container.height = current_cargoPlane.y_axis;
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
        printf("Container %d: Quantity: %d, Height: %d\n",
               i + 1, plane.containers[i].container_id, plane.containers[i].quantity, plane.containers[i].height);
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
}
