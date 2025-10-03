#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mqueue.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>

#define MSG_SIZE 100

//settings of POSIX msg queue
static struct mq_attr attr; 

void judge(unsigned int runners_num, mqd_t* runners_qid_arr);
void runner(unsigned int runners_num, unsigned int self_num, mqd_t* runners_qid_arr);

int main(int argc, char* argv[])
{
    unsigned int runners_num = 0;

    if (argv[1] != NULL){
        runners_num = atoi(argv[1]);
    }       
    else {
        printf("no arguments provided\n");
        exit(0);
    }

    //Runners
    char qname[32];
    attr.mq_maxmsg = runners_num;
    attr.mq_msgsize = MSG_SIZE;
    mqd_t* runners_qid_arr = (mqd_t*)malloc((runners_num + 1) * sizeof(mqd_t));
    for (unsigned int i = 0; i < runners_num + 1; i++)
    {
        snprintf(qname, 32, "/runner%d", i);
        runners_qid_arr[i] = mq_open(qname, O_RDWR | O_CREAT | O_NONBLOCK, 0666, &attr); //create msg_queue to connect next runner
        if (runners_qid_arr[i] == -1)
        {
            perror("mq_open");
            exit(1);
        }
    }    

    pid_t pid;
    for (unsigned int i = 0; i < runners_num + 1; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            perror("fork");
            exit(1);
        }
        else if (pid == 0) //child
        {
            if (i == 0) {
                judge(runners_num, runners_qid_arr);
            }
            else {
                runner(runners_num, i, runners_qid_arr);
            }

            exit(0);
        }
    }

    for (unsigned int i = 0; i < runners_num + 1; i++)
        wait(NULL);

    //close runners msg queues
    char name[30];
    for (int i = 0; i < runners_num + 1; i++)
    {
        if (mq_close(runners_qid_arr[i]) == -1){
            perror("mq_close");
            exit(1);
        }

        snprintf(name, 30, "/runner%d", i);
        
        if (mq_unlink(name) == -1){
            perror("mq_unlink");
            exit(1);
        }
    }
    free(runners_qid_arr);

    return 0;
}


void judge(unsigned int runners_num, mqd_t* runners_qid_arr)
{
    printf("Judge:     I wait for runners...\n");

    //Greet runners
    char text[MSG_SIZE];
    int runners_count = 0;
    unsigned int runner_id = 0;
    for (int i = 1; i < runners_num + 1; i++)
    {
        while (runners_count < runners_num)
        {
            if (mq_receive(runners_qid_arr[0], text, MSG_SIZE, &runner_id) == -1)
            {
                if (errno == EAGAIN){
                    continue;
                }
                else{
                    exit(1);
                }
            }
            else
            {
                printf("Judge:     I met runner #%u \n", runner_id);
                runners_count++;
            }
        }
    }

    //Give command to first runner
    printf("Judge:     Command to runner #0: run!\n");

    while (mq_send(runners_qid_arr[1], text, MSG_SIZE, 0) == -1) //priority has a meaning of sender's number
    {
        if (errno == EAGAIN){
            // perror("");
            continue;
        }
        else{
            exit(1);
        }
    }

    //begin counting time
    struct timeval tv1 = {};
    gettimeofday(&tv1, NULL);   

    //Receive command from last runner(last runner sends msg with type == runners_num + 1)
    while (mq_receive(runners_qid_arr[0], text, MSG_SIZE, &runner_id) == -1){
        if (errno == EAGAIN){
            continue;
        }
        else{
            exit(1);
        }
    }
    printf("Judge:     met last runner\n");

    //calculate time 
    struct timeval tv2 = {};
    gettimeofday(&tv2, NULL);

    long int time_sec = (long int)(tv2.tv_sec - tv1.tv_sec);
    long int time_usec = ((tv2.tv_usec - tv1.tv_usec));
    if (tv2.tv_usec < tv1.tv_usec)
    {
        time_sec -= 1;
        time_usec += 10e6; 
    }

    printf("Judge:  race finished, total time: = %ld.%ld s\n", time_sec, time_usec);
}


void runner(unsigned int runners_num, unsigned int self_num, mqd_t* runners_qid_arr)
{
    //Introduce
    printf("Runner #%d: I am #%d, came to stadion\n", self_num, self_num);
    
    //Greet the judge
    char text[MSG_SIZE];
    mq_send(runners_qid_arr[0], text, MSG_SIZE, (unsigned int)(self_num));

    //Receive command from judge/prev runner
    unsigned int runner_id = 0;
    printf("Runner #%u: waiting for command\n", self_num);

    while (mq_receive(runners_qid_arr[self_num], text, MSG_SIZE, &runner_id) == -1) 
    {
        if (errno == EAGAIN){
            continue;
        }
        else{
            exit(1);
        }
    }

    if (runner_id == 0)
    {
        printf("Runner #%d: received command from judge to run\n", self_num);
    }
    else 
    {
        printf("Runner #%d: received command from #%u to run\n", self_num, runner_id);
    }

    //Transfer queue(send message with self_number + 1)
    if (self_num != runners_num)
    {
        printf("Runner #%d: transfering command to run to next runner\n", self_num);
        mq_send(runners_qid_arr[self_num + 1], text, MSG_SIZE, (unsigned int)(self_num));
    }
    else 
    {
        printf("Runner #%d: transfering command judge\n", self_num);
        mq_send(runners_qid_arr[0], text, MSG_SIZE, (unsigned int)(self_num));
    }
}

