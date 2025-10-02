#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/ipc.h>

// #define RUNNER_MSG_TYPE 7
#define MSG_SIZE 100

struct msgbuf {
    long mtype; /* message type, must be > 0 */
    char mtext[MSG_SIZE]; /* message data */
};

void judge(int runners_num, int msg_id);
void runner(int runners_num, int self_num, int msg_id);

int get_msg(int qid, int msg_type, struct msgbuf* msg);
void send_msg(int qid, int msg_type, char* text);

int main(int argc, char* argv[])
{
    int runners_num = 0;

    if (argv[1] != NULL){
        runners_num = atoi(argv[1]);
    }       
    else {
        printf("no arguments provided\n");
        exit(0);
    }

    int qid = msgget(IPC_PRIVATE, IPC_CREAT | 0666); //create msg_queue
    if (qid == -1)
    {
        perror("error in creating msg queue");
        exit(1);
    }

    pid_t pid;

    pid = fork();
    if (pid < 0)
    {
        perror("error in fork");
        exit(1);
    }
    else if (pid == 0) //child
    {
        judge(runners_num, qid);
        exit(0);
    }

    for (int i = 1; i < runners_num + 1; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            perror("error in fork");
            exit(1);
        }
        else if (pid == 0) //child
        {
            runner(runners_num, i, qid);
            exit(0);
        }
    }

    for (int i = 0; i < runners_num + 1; i++)
        wait(NULL);

    if (msgctl(qid, IPC_RMID, 0) == -1){
        perror("error in removing msg queue");
        exit(1);
    }

    return 0;
}


void judge(int runners_num, int qid)
{
    printf("I am judge, waiting for runners...\n");

    struct msgbuf msg;

    int runners_count = 0;
    while (runners_count < runners_num)
    {
        if (get_msg(qid, -runners_num, &msg) == -1)
        {
            continue;
        }
        else 
        {
            printf("Judge: I met runner #%s \n", msg.mtext);
            runners_count += 1;
        }
    }
}


void runner(int runners_num, int self_num, int qid)
{
    printf("I am runnner #%d, came to stadion\n", self_num);
    
    //number of runner in text
    char text[MSG_SIZE];
    snprintf(text, MSG_SIZE, "%d", self_num); //send messaage with his self number

    send_msg(qid, self_num, text);
}


int get_msg(int qid, int msg_type, struct msgbuf* msg)
{
    if (msgrcv(qid, msg, sizeof(msg->mtext), msg_type,
        MSG_NOERROR | IPC_NOWAIT) == -1) 
    {
        if (errno != ENOMSG) 
        {
            perror("msgrcv");
            exit(1);
        }

        return -1; //no messages
        // printf("No message available for msgrcv()\n");
    } 

    return 0;
}


void send_msg(int qid, int msg_type, char* text)
{
    struct msgbuf msg;
    msg.mtype = msg_type;
    snprintf(msg.mtext, sizeof(msg.mtext), "%s", text);
    
    // send msg to queue
    if (msgsnd(qid, &msg, sizeof(msg.mtext), IPC_NOWAIT) == -1)
    {
        perror("error msgsnd");
        exit(1);
    }
}
