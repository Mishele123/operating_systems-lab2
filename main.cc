#include <signal.h>
#include <iostream>
#include <random>
#include <unistd.h>
#include <wait.h>
#include "check.hpp"


#define MIN_VALUE 1
#define MAX_VALUE 10
#define NUM_ROUNDS 30
#define TIME_DELAY 1


volatile sig_atomic_t lastSig;
volatile sig_atomic_t value;


void playerHandler(int sig, siginfo_t *si, void *ctx) 
{
    lastSig = sig;
    value = si->si_value.sival_int;
}


void handler(int sig) 
{
    lastSig = sig;
}


void waitSignal(pid_t hostPID, const sigset_t &signalMask) 
{
    while (true) 
    {
        sigsuspend(&signalMask);

        if (lastSig != SIGALRM)
            break;

    }
}


int genRandomNumber() 
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> distr(MIN_VALUE, MAX_VALUE);
    return distr(gen);
}



void setupHandlers() 
{
    struct sigaction rightAction{}, notRightAction{}, playerAction{};
    rightAction.sa_handler = handler;
    check(sigaction(SIGUSR1, &rightAction, NULL));

    notRightAction.sa_handler = handler;
    check(sigaction(SIGUSR2, &notRightAction, NULL));

    playerAction.sa_sigaction = playerHandler;
    playerAction.sa_flags = SA_SIGINFO;
    check(sigaction(SIGRTMAX, &playerAction, NULL));
}



void playPlayer(pid_t hostPID, const sigset_t &signalMask) 
{
    waitSignal(hostPID, signalMask);

    int numTries = 1;
    for (int el = MAX_VALUE; el > 0; el--) 
    {
        check(sigqueue(hostPID, SIGRTMAX, sigval{el}));
        std::cout << "PID [" << getpid() << "] think it's " << el << std::endl;

        waitSignal(hostPID, signalMask);

        if (lastSig == SIGUSR1)
        {
            std::cout << "That's right, it's " << el << std::endl;
            break;
        }
        numTries++;
    }
    std::cout << "Number of attempts: " << numTries << " tries" << std::endl;
}



void playHost(pid_t hostPID, int round, const sigset_t &signalMask) 
{
    const int random_num = genRandomNumber();
    std::cout << "\nRound " << round << std::endl;
    std::cout << "[PID " << getpid() << "] I guessed a number from " << MIN_VALUE << " to " << MAX_VALUE << std::endl;

    check(kill(hostPID, SIGUSR2));

    for (int i = MIN_VALUE; i <= MAX_VALUE; i++)
    {
        waitSignal(hostPID, signalMask);
        if (value == random_num) 
        {
            check(kill(hostPID, SIGUSR1));
            break;
        }
        check(kill(hostPID, SIGUSR2));
    }
}



void play(pid_t pid, pid_t parentPID, int round, const sigset_t &signalMask) 
{
    if (round % 2 == 0) 
        (pid == 0) ? playHost(parentPID, round, signalMask) : playPlayer(pid, signalMask);
    else 
        (pid > 0) ? playHost(pid, round, signalMask) : playPlayer(parentPID, signalMask);
}



int main() 
{
    setupHandlers();

    sigset_t set;
    sigfillset(&set);
    check(sigprocmask(SIG_BLOCK, &set, &set));
    check(sigdelset(&set, SIGUSR1));
    check(sigdelset(&set, SIGUSR2));
    check(sigdelset(&set, SIGRTMAX));
    check(sigdelset(&set, SIGALRM));

    pid_t parentPID = getpid();
    pid_t pid = check(fork());

    for (int round = 1; round <= NUM_ROUNDS; round++) 
    {
        sleep(TIME_DELAY);
        play(pid, parentPID, round, set);
    }

    return 0;
}