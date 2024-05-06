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
    /*send a USR1 signal to the parent process with a random value
        1 => Killing a collector
        2 => Killing a distributer
    // */
    // union sigval value;
    // value.sival_int = rand() % 2 == 0 ? 1 : 2;
    // sigqueue(getppid(), SIGUSR1, value);
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
        int period = rand() % (20 - 10 + 1) + 10; // random period between 10 and 20
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