#include <fcntl.h>
#include <iostream>
#include <mqueue.h>
#include <random>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#include "check.hpp"



#define MIN_VALUE 1
#define MAX_VALUE 10
#define NUM_ROUNDS 10
#define WAIT_TIME 2


const int SIG_WIN = -1;
const int SIG_FAIL = -2;



void send(mqd_t queue, const int* value, pid_t hostPID) 
{
    timespec timeout{};
    while (true) 
    {
        check(clock_gettime(CLOCK_REALTIME, &timeout));
        timeout.tv_sec += WAIT_TIME;
        if (check_except(mq_timedsend(queue, (char*)value, sizeof(int), 0, &timeout), ETIMEDOUT) == 0) 
        {
            break;
        }
    }
}


void receive(mqd_t queue, int* value, pid_t hostPID) 
{
    timespec timeout{};
    while (true) 
    {
        check(clock_gettime(CLOCK_REALTIME, &timeout));
        timeout.tv_sec += WAIT_TIME;
        if (check_except(mq_timedreceive(queue, (char*)value, sizeof(int), nullptr, &timeout), ETIMEDOUT) >= 0) 
        {
            break;
        }
    }
}


int genRandomNumber() 
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> distr(MIN_VALUE, MAX_VALUE);
    return distr(gen);
}


void playPlayer(mqd_t mq_write, mqd_t mq_read, pid_t hostPID) 
{
    int count = 0;
    int buffer = 0;

    for (int value = MAX_VALUE; value > 0; --value) 
    {
        receive(mq_read, &buffer, hostPID);

        if (buffer == SIG_FAIL) 
        {
            std::cout << "PID [" << getpid() << "] value = " << value << std::endl;
            count++;
            buffer = value;
            send(mq_write, &buffer, hostPID);
        }
        else if (buffer == SIG_WIN) 
        {
            std::cout << "PID [" << getpid() << "] need " << count << " attempts to win" << std::endl;
            break;
        }
    }
}


void playHost(mqd_t mq_write, mqd_t mq_read, int round, pid_t hostPID) 
{
    std::cout << "\nRound " << round << std::endl;

    const int number = genRandomNumber();

    std::cout << "PID [" << getpid() << "] wish a number = " << number << std::endl;

    int buffer = SIG_FAIL;
    send(mq_write, &buffer, hostPID);

    while (true) 
    {
        receive(mq_read, &buffer, hostPID);

        if (buffer == number) 
        {
            buffer = SIG_WIN;
            send(mq_write, &buffer, hostPID);
            std::cout << "YEEEES. YOU DID IT!!!" << std::endl;
            break;
        }
        std::cout << "PID [" << getpid() << "] did not guess." << std::endl;
        buffer = SIG_FAIL;
        send(mq_write, &buffer, hostPID);
    }
}


void play(mqd_t mq_p_to_c, mqd_t mq_c_to_p, int round, pid_t parentPID, pid_t pid) 
{
    if (pid != 0) 
        (round % 2 != 0) ? playHost(mq_p_to_c, mq_c_to_p, round, pid)
                          : playPlayer(mq_p_to_c, mq_c_to_p, pid);
    else
        (round % 2 != 0) ? playPlayer(mq_c_to_p, mq_p_to_c, parentPID)
                          : playHost(mq_c_to_p, mq_p_to_c, round, parentPID);
}


int main() 
{

    const char QUEUE_PARENT_TO_CHILD[] = "/mq_p_to_c";
    const char QUEUE_CHILD_TO_PARENT[] = "/mq_c_to_p";

    mq_unlink(QUEUE_PARENT_TO_CHILD);
    mq_unlink(QUEUE_CHILD_TO_PARENT);

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 1;
    attr.mq_msgsize = sizeof(int);

    mqd_t mq_p_to_c = check(mq_open(QUEUE_PARENT_TO_CHILD, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, &attr));
    mqd_t mq_c_to_p = check(mq_open(QUEUE_CHILD_TO_PARENT, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, &attr));

    pid_t parentPID = getpid();
    pid_t pid = check(fork());

    for (int round = 1; round <= NUM_ROUNDS; round++) 
    {
        sleep(WAIT_TIME);
        play(mq_p_to_c, mq_c_to_p, round, parentPID, pid);
    }

    if (pid > 0) 
    {
        check(mq_close(mq_p_to_c));
        check(mq_close(mq_c_to_p));
        check(mq_unlink(QUEUE_PARENT_TO_CHILD));
        check(mq_unlink(QUEUE_CHILD_TO_PARENT));
    }

    return 0;
}


// транспонировать матрицы в 3 лабе перед умножением
