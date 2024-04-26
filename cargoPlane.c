#include "local.h"
#include "functions.c"

void initialize_cargo_plane();
void print_cargo_plane(cargoPlane plane);
void signalHandler(int sig);
void signalSetup();
void writeSHM();

cargoPlane current_cargoPlane;
int planeID, numOfContainers, minDropFreq, maxDropFreq, minRefill, maxRefill;
int shmid;

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

    // Initialize the cargo plane
    signalSetup();
    initialize_cargo_plane();
    // printf("Cargo Plane %d has been created\n", current_cargoPlane.plane_id);

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
        // // Simulate the cargo plane operations
        // printf("Cargo Plane %d starting operations...\n", current_cargoPlane.plane_id);
        // printf("----------------------------------------------------------\n");
        // print_cargo_plane(current_cargoPlane);
        writeSHM(); // write the containers to shared memory
    }
}

void writeSHM()
{
    printf("Cargo Plane %d writing to SHM\n", current_cargoPlane.plane_id);
    shmid = openSHM(SHM_PLANES, SHM_SIZE);
    if (shmid == -1)
    {
        perror("openSHM Cargo Plane Error");
        exit(1);
    }
    void *shm_ptr = mapSHM(shmid, SHM_SIZE);
    if (shm_ptr == NULL)
    {
        perror("mapSHM Cargo Plane Error");
        exit(1);
    }
    printf("Cargo Plane Opened SHM\n");
    // write the containers to shared memory
    for (int i = 0; i < current_cargoPlane.numContainers; i++)
    {
        // cast the shared memory pointer to the FlourContainer struct
        printf("Writing container %d to SHM\n", i + 1);
        memcpy(shm_ptr, &current_cargoPlane.containers[i], sizeof(FlourContainer));
        shm_ptr += sizeof(FlourContainer);
    }
}

void initialize_cargo_plane()
{
    srand(time(NULL) ^ (getpid() << 16)); // Seed random generator based on planeID
    current_cargoPlane.plane_pid = getpid();
    current_cargoPlane.plane_id = planeID;
    current_cargoPlane.numContainers = numOfContainers;
    current_cargoPlane.status = -1;
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
}
