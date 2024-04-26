#ifndef LOCAL_H
#define LOCAL_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>

// Maximum values for simulation parameters
#define MAX_CONTAINERS 300
#define MAX_CARGOPLANES 50
#define MAX_RELIEF_WORKERS 100
#define MAX_FAMILIES 50

#define SHM_PLANES "/MEM1"
#define SHM_SIZE 1024
#define SHM_DATA "/MEM2"
#define SHM_DATA_SIZE 512
#define SEM_CONTAINERS "/SEM1"

typedef struct
{
    int totalContainersDropped;
    int cleectedContainers;
} SharedData;

// Structure to represent a flour container
typedef struct
{
    int container_id;
    int quantity;
    int height; // -1: damaged totally, 0: have reached the ground, otherwise: height from the ground
} FlourContainer;

// Shared memory structure for wheat flour distribution
struct WheatFlourDistribution
{
    int totalContainersDropped;
    FlourContainer containers[MAX_CONTAINERS];
    int refillTimes;
};

// Updated structure for a cargo plane
typedef struct
{
    pid_t plane_pid;                           // Unique identifier for each cargo plane
    int plane_id;                              // Plane ID
    int numContainers;                         // Number of containers currently loaded on the plane
    FlourContainer containers[MAX_CONTAINERS]; // Array of containers
    int status;                                // Status of the cargo plane (0: In transit, 1: Dropping, 2: Refilling)
    int dropFrequency;                         // Frequency of dropping containers (in seconds)
    int valid;                                 // 0: invalid, 1: vaid
    int y_axis;                                // y-axis of the plane
    int x_axis;                                // x-axis of the plane
} cargoPlane;

// Signal handler for cargo plane operations
void signalHandler(int sig);

#endif // LOCAL_H
