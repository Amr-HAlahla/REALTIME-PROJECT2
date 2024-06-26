#include "local.h"
#include "functions.c"

void setupSignals();
void signalHandler(int sig);
void open_shm_sem();
void updateStarvationLevel();
void reorderFamilies();

// families shared data
int families_shmid;
int *families_shm_ptr;
sem_t *sem_families;

int NUM_OF_FAMILIES;                   /* number of families */
int FAMILY_STARVATION_DEATH_THRESHOLD; /* starvation level that will cause the family to die */
int FAMILY_DIED_THRESHOLD;             /* Threshold of number of familes that will cause the program to terminate */
Family **FAMILIES;
int UP_TO_DATE = 1;
int num_of_died_families = 0;
int CRITICAL = 0;

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <num_of_families> <FAMILY_STARVATION_DEATH_THRESHOLD> <FAMILY_DIED_THRESHOLD>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    NUM_OF_FAMILIES = atoi(argv[1]);
    FAMILY_STARVATION_DEATH_THRESHOLD = atoi(argv[2]);
    FAMILY_DIED_THRESHOLD = atoi(argv[3]);
    FAMILIES = malloc(sizeof(Family *) * NUM_OF_FAMILIES);
    srand(time(NULL) ^ (getpid() << 16));
    for (int i = 0; i < NUM_OF_FAMILIES; i++)
    {
        Family *family = malloc(sizeof(Family));
        family->id = i;
        family->needed_bags = rand() % (15 - 5 + 1) + 5;     // random number of bags between 5 and 15
        family->starvation_level = rand() % (5 - 1 + 1) + 1; // random number of starvation level between 1 and 5
        family->history = 0;
        // ratio field => decide how much will increase and decrease the starvation level and the needed bags
        family->ratio = ((float)rand() / RAND_MAX) * (0.4 - 0.1) + 0.1;
        family->alive = 1;
        family->pid = getpid();
        printf("Family %d | Needed Bags = %d | Starvation Level = %d | Ratio = %.2f\n", i, family->needed_bags, family->starvation_level, family->ratio);
        FAMILIES[i] = family;
    }
    printf("%d Families have been created\n", NUM_OF_FAMILIES);
    setupSignals();
    open_shm_sem();
    while (1)
    {
        pause();
    }
    return 0;
}

void reorderFamilies()
{
    // reorder families based on the starvation level ( from the most starved to the least starved ).
    if (sem_wait(sem_families) == -1)
    {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    CRITICAL = 1;
    if (!UP_TO_DATE)
    {
        // read the families from the shared memory into the local families array, to ensure that the families are up to date
        for (int i = 0; i < NUM_OF_FAMILIES; i++)
        {
            Family *family = malloc(sizeof(Family));
            memcpy(family, families_shm_ptr + i * sizeof(Family), sizeof(Family));
            FAMILIES[i] = family;
        }
        UP_TO_DATE = 1;
    }
    printf("Reordering families\n"); // using bubble sort
    for (int i = 0; i < NUM_OF_FAMILIES; i++)
    {
        for (int j = i + 1; j < NUM_OF_FAMILIES; j++)
        {
            if (FAMILIES[i]->starvation_level < FAMILIES[j]->starvation_level)
            {
                Family *temp = FAMILIES[i];
                FAMILIES[i] = FAMILIES[j];
                FAMILIES[j] = temp;
            }
        }
    }
    printf("Families have been reordered\n");
    for (int i = 0; i < NUM_OF_FAMILIES; i++)
    {
        printf("Family %d | Needed Bags = %d | Starvation Level = %d | History = %d\n | ALIVE = %d\n",
               i, FAMILIES[i]->needed_bags, FAMILIES[i]->starvation_level, FAMILIES[i]->history, FAMILIES[i]->alive);
    }
    // now write the families info into the shared memory of families
    for (int i = 0; i < NUM_OF_FAMILIES; i++)
    {
        memcpy(families_shm_ptr + i * sizeof(Family), FAMILIES[i], sizeof(Family));
    }
    printf("Families have been written into the shared memory after reordering\n");
    if (sem_post(sem_families) == -1)
    {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
    CRITICAL = 0;
}

void updateStarvationLevel()
{
    if (sem_wait(sem_families) == -1)
    {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    CRITICAL = 1;
    printf("Updating the starvation level\n");
    for (int i = 0; i < NUM_OF_FAMILIES; i++)
    {
        /* update the starvation level, and check if the family is still alive */
        if (FAMILIES[i]->alive)
        {
            if (FAMILIES[i]->starvation_level < 10)
            {
                FAMILIES[i]->starvation_level += 5 * (1 + FAMILIES[i]->ratio);
            }
            else
            {
                FAMILIES[i]->starvation_level += FAMILIES[i]->starvation_level * FAMILIES[i]->ratio;
            }
            FAMILIES[i]->needed_bags += 35 * FAMILIES[i]->ratio;
            // check if the family has died
            if (FAMILIES[i]->starvation_level >= FAMILY_STARVATION_DEATH_THRESHOLD)
            {
                FAMILIES[i]->alive = 0;
                FAMILIES[i]->starvation_level = -1;
                num_of_died_families++;
                printf("Family %d has died, starvation level = %d, and Number of died families = %d\n", i, FAMILIES[i]->starvation_level, num_of_died_families);
                // check if the number of died families has reached the threshold
                if (num_of_died_families >= FAMILY_DIED_THRESHOLD)
                {
                    printf("The number of died families %d, has reached the threshold %d\n", num_of_died_families, FAMILY_DIED_THRESHOLD);
                    kill(getppid(), SIGTERM);
                }
            }
        }
    }
    if (sem_post(sem_families) == -1)
    {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
    CRITICAL = 0;
    reorderFamilies();
}

void signalHandler(int sig)
{
    if (sig == SIGALRM)
    {
        /* update the starvation level, and check if the family is still alive */
        updateStarvationLevel();
        int period = rand() % (12 - 5 + 1) + 5;
        alarm(period);
    }
    else if (sig == SIGTSTP)
    {
        printf("Families received SIGTSTP\n");
        if (CRITICAL)
        {
            if (sem_post(sem_families) == -1)
            {
                perror("sem_post");
            }
        }
        for (int i = 0; i < NUM_OF_FAMILIES; i++)
        {
            printf("Family %d | Needed Bags = %d | Starvation Level = %d | History = %d | ALIVE = %d\n",
                   i, FAMILIES[i]->needed_bags, FAMILIES[i]->starvation_level, FAMILIES[i]->history, FAMILIES[i]->alive);
        }
        printf("Families are terminated\n");
        for (int i = 0; i < NUM_OF_FAMILIES; i++)
        {
            free(FAMILIES[i]);
        }
        free(FAMILIES);
        exit(EXIT_SUCCESS);
    }
    else if (sig == SIGUSR1)
    {
        // wheat flour bags received
        printf("Wheat flour bags have been received\n");
        UP_TO_DATE = 0;
        reorderFamilies();
    }
}

void setupSignals()
{
    if (sigset(SIGALRM, signalHandler) == SIG_ERR)
    {
        perror("SIGALRM");
        exit(EXIT_FAILURE);
    }

    if (sigset(SIGTSTP, signalHandler) == SIG_ERR)
    {
        perror("SIGTSTP");
        exit(EXIT_FAILURE);
    }

    if (sigset(SIGUSR1, signalHandler) == SIG_ERR)
    {
        perror("SIGUSR1");
        exit(EXIT_FAILURE);
    }
}

void open_shm_sem()
{
    // Open the shared memory segment
    if ((families_shmid = shm_open(FAMILIES_SHM, O_RDWR, 0666)) == -1)
    {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    // Attach to the shared memory
    if ((families_shm_ptr = mmap(0, FAMILIES_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, families_shmid, 0)) == MAP_FAILED)
    {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Open the semaphore
    if ((sem_families = sem_open(SEM_FAMILIES, 0)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    printf("Families shared memory and semaphore have been opened\n");
}
