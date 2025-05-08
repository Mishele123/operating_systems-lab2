#include <signal.h>
#include <iostream>
#include <random>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>

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

void waitSignal(pid_t pid, const sigset_t &signalMask, bool isHost) 
{
    while (true) 
    {
        sigsuspend(&signalMask);
        if ((lastSig == SIGUSR1 || lastSig == SIGUSR2) & isHost == false || lastSig == SIGRTMAX & isHost == true)
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

void playPlayer(pid_t hostPID) 
{
    sigset_t signalMask, oldMask, suspendMask;

    check(sigemptyset(&signalMask));
    check(sigaddset(&signalMask, SIGRTMAX));
    check(sigprocmask(SIG_BLOCK, &signalMask, &oldMask));

    check(sigemptyset(&suspendMask));

    waitSignal(hostPID, suspendMask, false);

    int numTries = 1;
    for (int el = MAX_VALUE; el >= MIN_VALUE; el--) 
    {
        check(sigqueue(hostPID, SIGRTMAX, sigval{.sival_int = el}));
        std::cout << "PID [" << getpid() << "] think it's " << el << std::endl;

        waitSignal(hostPID, suspendMask, false);

        if (lastSig == SIGUSR1) 
        {
            std::cout << "That's right, it's " << el << std::endl;
            break;
        }
        numTries++;
    }
    std::cout << "Number of attempts: " << numTries << " tries" << std::endl;

    check(sigprocmask(SIG_SETMASK, &oldMask, nullptr));
}

void playHost(pid_t playerPID, int round) 
{
    sigset_t signalMask, oldMask, suspendMask;

    check(sigemptyset(&signalMask));
    check(sigaddset(&signalMask, SIGUSR1));
    check(sigaddset(&signalMask, SIGUSR2));
    check(sigprocmask(SIG_BLOCK, &signalMask, &oldMask));

    check(sigemptyset(&suspendMask));

    const int random_num = genRandomNumber();
    std::cout << "\nRound " << round << std::endl;
    std::cout << "[PID " << getpid() << "] I guessed a number from " << MIN_VALUE << " to " 
    << MAX_VALUE << std::endl;
    std::cout << "Random value = " << random_num << std::endl;
    check(kill(playerPID, SIGUSR2));

    for (int i = MIN_VALUE; i <= MAX_VALUE; i++) 
    {
        waitSignal(playerPID, suspendMask, true);
        if (value == random_num) 
        {
            check(kill(playerPID, SIGUSR1));
            break;
        }
        check(kill(playerPID, SIGUSR2));
    }

    check(sigprocmask(SIG_SETMASK, &oldMask, nullptr));
}

void play(pid_t pid, pid_t parentPID, int round) 
{
    if (round % 2 == 0) 
    {
        (pid == 0) ? playHost(parentPID, round) : playPlayer(pid);
    } else 
    {
        (pid > 0) ? playHost(pid, round) : playPlayer(parentPID);
    }
}

int main() 
{
    sigset_t baseMask, oldMask;
    check(sigemptyset(&baseMask));
    check(sigaddset(&baseMask, SIGUSR1));
    check(sigaddset(&baseMask, SIGUSR2));
    check(sigaddset(&baseMask, SIGRTMAX));
    check(sigprocmask(SIG_BLOCK, &baseMask, &oldMask));

    setupHandlers();

    pid_t parentPID = getpid();
    pid_t pid = check(fork());

    for (int round = 1; round <= NUM_ROUNDS; round++) 
    {
        sleep(TIME_DELAY);
        play(pid, parentPID, round);
    }

    check(sigprocmask(SIG_SETMASK, &oldMask, nullptr));


    return 0;
}