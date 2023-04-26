#define _XOPEN_SOURCE 700

#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using std::cout;
using std::endl;
using std::string;
using std::to_string;

#define ALLOW 1
#define DENY 0
#define REQUEST_INFO 2
#define IS_READY 3

#define FIFO_NAME "child_info_holder"

volatile sig_atomic_t counter = 0;

volatile sig_atomic_t stat_x = -1;
volatile sig_atomic_t stat_y = -1;
volatile sig_atomic_t fifo_fd;
volatile char *child_name;

volatile struct Pair
{
    int x = -1;
    int y = -1;
} pair;

void set_pair(volatile Pair &pair)
{
    static bool is_one = false;

    if (is_one)
    {
        pair.x = 1;
        pair.y = 1;
    }
    else
    {
        pair.x = 0;
        pair.y = 0;
    }

    is_one = !is_one;
}

void collect_statistics(volatile Pair &pair)
{
    stat_x = pair.x;
    stat_y = pair.y;
}

void print_statistics()
{
    string information = "\nCHILD[" + string(const_cast<char *>(child_name));
    information.append("]: pid=" + to_string(getpid()) + " ppid=" + to_string(getppid()) + " stat[" + to_string(stat_x) + ';' + to_string(stat_y) + "] pair[" + to_string(pair.x) + ';' + to_string(pair.y) + "]\n");
    sigval is_ready;
    is_ready.sival_int = IS_READY;
    if (write(fifo_fd, information.c_str(), information.size() + 1) == -1)
    {
        perror("write");
        exit(EXIT_FAILURE);
    }
    printf("\nsending is_ready\n");
    sigqueue(getppid(), SIGUSR1, is_ready);
}

void handle_SIGALRM(int signum, siginfo_t *info, void *context)
{
    ++counter;
    collect_statistics(pair);
}

void handle_SIGUSR2(int signum, siginfo_t *info, void *context)
{
    if (info->si_value.sival_int == ALLOW || info->si_value.sival_int == REQUEST_INFO)
        print_statistics();
}

int main(int argc, char **argv)
{
    if ((fifo_fd = open(FIFO_NAME, O_WRONLY)) == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // printf(": child process %d of parent process %d was created\n", getpid(), getppid());
    child_name = argv[0];
    struct timespec interval, remaining;
    interval.tv_sec = 2;
    interval.tv_nsec = 0;

    struct sigaction action_sigalrm;
    action_sigalrm.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action_sigalrm.sa_mask);
    action_sigalrm.sa_sigaction = handle_SIGALRM;

    struct sigaction action_sigusr2;
    action_sigusr2.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action_sigusr2.sa_mask);
    action_sigusr2.sa_sigaction = handle_SIGUSR2;
    sigval request;

    if (sigaction(SIGALRM, &action_sigalrm, NULL) == -1 || sigaction(SIGUSR2, &action_sigusr2, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    while (true)
    {
        nanosleep(&interval, &remaining);
        sigqueue(getpid(), SIGALRM, request);

        set_pair(pair);

        if (counter % 10 == 0 && counter > 0)
            sigqueue(getppid(), SIGUSR1, request);
    }

    exit(EXIT_SUCCESS);
}