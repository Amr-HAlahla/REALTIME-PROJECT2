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

// Maximum values for simulation parameters
#define MAX_CONTAINERS 300
#define MAX_CARGOPLANES 50
#define MAX_RELIEF_WORKERS 100
#define MAX_FAMILIES 50

#define SHM_PLANES "/MEM1"
#define SHM_SIZE 1024

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

// Semaphore operations for acquiring and releasing semaphores
void acquireSem(int semid, int semnum);
void releaseSem(int semid, int semnum);

// Signal handler for cargo plane operations
void signalHandler(int sig);

// Semaphore union as required by semctl
union semun
{
    int val;               // Value for SETVAL
    struct semid_ds *buf;  // Buffer for IPC_STAT, IPC_SET
    unsigned short *array; // Array for GETALL, SETALL
};

// Sembuf structures for semaphore operations
struct sembuf acquire = {0, -1, SEM_UNDO},
              release = {0, 1, SEM_UNDO};

// Define semaphore indices
enum
{
    SEM_AVAILABLE,
    SEM_FILLED
};

#endif // LOCAL_H
