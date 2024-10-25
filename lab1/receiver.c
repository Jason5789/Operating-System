#include "receiver.h"

void receive(message_t *message, mailbox_t *mailbox){
    /*  TODO: 
        1. Use flag to determine the communication method
        2. According to the communication method, receive the message
    */
    if (mailbox->flag == 1){
        msgrcv(mailbox->storage.msqid, message, 1024, 1, 0);
    }
    else{
        strcpy(message->mtext, mailbox->storage.shm_addr);
    }
}

int main(int argc, char *argv[]){
    /*  TODO: 
        1) Call receive(&message, &mailbox) according to the flow in slide 4
        2) Measure the total receiving time
        3) Get the mechanism from command line arguments
            • e.g. ./receiver 1
        4) Print information on the console according to the output format
        5) If the exit message is received, print the total receiving time and terminate the receiver.c
    */
    if (argc != 2) {
        printf(stderr, "Usage: %s <mechanism>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    mailbox_t mailbox;
    int share;
    int mechanism = atoi(argv[1]);
    mailbox.flag = mechanism;
    
    if (mechanism == 1) {
        printf("Message Passing\n");
        mailbox.storage.msqid = msgget(ftok("msgqueue",65), 0666 | IPC_CREAT);
    } 
    else{
        printf("Shared memory\n");
        share = shmget(ftok("shmfile", 65), 1024, 0666 | IPC_CREAT);
        mailbox.storage.shm_addr = (char *)shmat(share,NULL,0);
    }

    message_t message;

    // 打開信號量
    sem_t* s_sem = sem_open("/s_sem", O_CREAT, 0644, 0);
    sem_t* r_sem = sem_open("/r_sem", O_CREAT, 0644, 0);
    double time = 0.0;
    
    do{ 
        struct timespec start, end;
        if(sem_wait(r_sem) == 0){
            clock_gettime(CLOCK_MONOTONIC, &start);
            receive(&message, &mailbox);
            clock_gettime(CLOCK_MONOTONIC, &end);
        }
        if (strcmp(message.mtext, "EOF") == 0) {
            break;
        }
        printf("Receiving message: %s", message.mtext);
        time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        sem_post(s_sem);
    }while(1);
        
    printf("\nSender exit!\nTotal time taken in receiving msg: %f s\n", time);

    if (mechanism == 1){
        msgctl(mailbox.storage.msqid, IPC_RMID, NULL);
    }
    else{
        shmdt(mailbox.storage.shm_addr);
        shmctl(share, IPC_RMID, NULL);
    }
    
    sem_close(s_sem);
    sem_close(r_sem);
    
    return 0;
}
