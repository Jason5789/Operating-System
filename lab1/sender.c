#include "sender.h"

void send(message_t message, mailbox_t *mailbox_ptr) {
    /*  TODO: 
        1. Use flag to determine the communication method
        2. According to the communication method, send the message
    */
    if (mailbox_ptr->flag == 1) {
        msgsnd(mailbox_ptr->storage.msqid, &message, sizeof(message.mtext), 0);
    } 
    else{
        strcpy(mailbox_ptr->storage.shm_addr, message.mtext);
    }
}

int main(int argc, char *argv[]) {
    /*  TODO: 
        1) Call send(message, &mailbox) according to the flow in slide 4
        2) Measure the total sending time
        3) Get the mechanism and the input file from command line arguments
            • e.g. ./sender 1 input.txt
                    (1 for Message Passing, 2 for Shared Memory)
        4) Get the messages to be sent from the input file
        5) Print information on the console according to the output format
        6) If the message from the input file is EOF, send an exit message to the receiver.c
        7) Print the total sending time and terminate the sender.c
    */
   
    if (argc != 3) {
        printf(stderr, "Usage: %s <mechanism> <input_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    mailbox_t mailbox;
    int mechanism = atoi(argv[1]);
    mailbox.flag = mechanism;
    char *input_file = argv[2];

    if(mechanism == 1){
        printf("Message Passing\n");
        mailbox.storage.msqid = msgget(ftok("msgqueue", 65), 0666 | IPC_CREAT);
    } 
    else{
        printf("Shared memory\n");
        key_t key = ftok("shmfile", 65);
        int shmid = shmget(key, SHM_SIZE, 0666 | IPC_CREAT);
        mailbox.storage.shm_addr = (char *)shmat(shmid, NULL, 0);
    }

    message_t message;
    sem_t* s_sem = sem_open("/s_sem", O_CREAT, 0644, 0);
    sem_t* r_sem = sem_open("/r_sem", O_CREAT, 0644, 0);
    
    struct timespec start, end;
    double time = 0.0;
    
    FILE *file = fopen(input_file, "r");
    while (fgets(message.mtext, sizeof(message.mtext), file) != NULL) {
        message.mtype = 1;
        clock_gettime(CLOCK_MONOTONIC, &start);
        send(message, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        printf("Sending message: %s", message.mtext);

        time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        sem_post(r_sem);
        sem_wait(s_sem);
    }

    strcpy(message.mtext, "EOF");
    send(message, &mailbox);
    sem_post(r_sem);
    fclose(file);
    printf("\nEnd of input file! exit!\n");
    printf("Total time taken in sending msg: %f s\n", time);

    sem_close(r_sem);
    sem_close(s_sem);

    return 0;
}