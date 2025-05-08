#include "check.hpp"
#include <unistd.h>
#include <ctime>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stack>
#include <iostream>
#include <errno.h>

void setting_mask() {
    sigset_t set;
    sigemptyset(&set);
    sigfillset(&set);
    check(sigprocmask(SIG_SETMASK, &set, nullptr));
}

bool process_exists(pid_t p) {
    return !(kill(p, 0) == -1 && errno == ESRCH);
}

int wait_signal(sigset_t &wait_set, siginfo_t &info, pid_t opponent) {
    struct timespec timeout = {0, 1000000}; 
    while (true) {
        int sig = sigtimedwait(&wait_set, &info, &timeout);
        if (sig >= 0)
            return sig;
        if (errno != EAGAIN)
            check(-1);
        if (!process_exists(opponent))
            exit(EXIT_SUCCESS);
    }
}

void left_player(int range_max, pid_t opponent, int round) {
    int number = rand() % range_max + 1;
    printf("Game: %d\n", round);
    printf("Chooser pid=%d, hidden number=%d\n", getpid(), number);

    check(sigqueue(opponent, SIGRTMAX, sigval{ range_max }));

    while (true) {
        siginfo_t info;
        sigset_t wait_set;
        sigemptyset(&wait_set);
        sigaddset(&wait_set, SIGRTMAX);
        wait_signal(wait_set, info, opponent);
        int guess = info.si_value.sival_int;

        if (guess == number) {
            check(kill(opponent, SIGUSR1));
            break;
        } else {
            check(kill(opponent, SIGUSR2));
        }
    }
}
void right_player(pid_t opponent, int round) {
    siginfo_t info;
    sigset_t wait_set;
    sigemptyset(&wait_set);
    sigaddset(&wait_set, SIGRTMAX);
    wait_signal(wait_set, info, opponent);
    int max_range = info.si_value.sival_int;

    std::stack<int> numbers;
    for (int i = 1; i <= max_range; i++)
        numbers.push(i);

    int count = 0;
    while (!numbers.empty()) {
        int value = numbers.top(); numbers.pop();
        check(sigqueue(opponent, SIGRTMAX, sigval{ value }));

        sigemptyset(&wait_set);
        sigaddset(&wait_set, SIGUSR1);
        sigaddset(&wait_set, SIGUSR2);
        int signo = wait_signal(wait_set, info, opponent);

        if (signo == SIGUSR1) {
            printf("Guesser pid=%d guessed: %d in %d tries\n", getpid(), value, count);
            break;
        }
        count++;
    }
}

void role_change(int round, pid_t child_pid, pid_t parent_pid, int range_max) {
    if (round % 2 == 1) {
        if (child_pid > 0)
            left_player(range_max, child_pid, round);
        else
            right_player(parent_pid, round);
    } else {
        if (child_pid > 0)
            right_player(child_pid, round);
        else
            left_player(range_max, parent_pid, round);
    }
    std::cout << std::endl;
}

void start(int rounds, int range_max) {
    setting_mask();
    pid_t parent_pid = getpid();
    pid_t child_pid = check(fork());
    srand(child_pid ? 1000 : 100000);

    for (int i = 1; i <= rounds; i++) {
        role_change(i, child_pid, parent_pid, range_max);
    }

    if (child_pid > 0) wait(nullptr);
}

int main() {
    start(100, 10000);
    return 0;
}