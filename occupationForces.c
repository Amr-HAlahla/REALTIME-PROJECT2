#include "local.h"
#include "functions.c"

void setupSignals();
void signalHandler(int sig);
void killWorker();

int main(int argc, char *argv[])
{
    if (argc != 1)
    {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    setupSignals();
    printf("Occupation Forces are here !!\n");
    while (1)
    {
        pause();
    }
    return 0;
}

void killWorker()
{
    printf("Occupation Forces are trying to kill a worker\n");
    kill(getppid(), SIGUSR1);
}

void signalHandler(int sig)
{
    if (sig == SIGTSTP)
    {
        printf("Occupation Forces are leaving the area\n");
        exit(EXIT_SUCCESS);
    }
    else if (sig == SIGALRM)
    {
        killWorker();
        int period = rand() % (20 - 8 + 1) + 8;
        alarm(period);
    }
}

void setupSignals()
{
    if (signal(SIGTSTP, signalHandler) == SIG_ERR)
    {
        perror("SIGTSTP");
        exit(EXIT_FAILURE);
    }
    if (signal(SIGALRM, signalHandler) == SIG_ERR)
    {
        perror("SIGALRM");
        exit(EXIT_FAILURE);
    }
}