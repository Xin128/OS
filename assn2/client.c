/* Lara Goxhaj          * 260574574 *
 * COMP 310/ECSE 427    * assn 2    *
 * -------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <semaphore.h>
#include "jobs.h"


// shared memory variables -----------------------------------------------------
int shm_fd;			// file descriptor, from shm_open()
void *shm_base;			// base address, from mmap()


// checks if command line arg is positive integer ------------------------------
int is_valid_num(char num[])
{
    int i=0;
    for (; num[i] != 0; i++)
        if (!isdigit(num[i]))
            return -1;
    return 0;
}


// release shared memory --------------------------------------------------------
void close_unlink()
{
    // removed mapped shared memory segment from the address space of the process
    if (munmap(shm_base, SIZE) == -1) {
	printf("cons: Unmap failed: %s\n", strerror(errno));
	exit(1);
    }
    // close shared memory segment as if it was a file --------------------------
    if (close(shm_fd) == -1) {
	printf("cons: Close failed: %s\n", strerror(errno));
	exit(1);
    }
}


void quitHandler(int signal) {
    if ((signal == SIGINT) || (signal == SIGQUIT) || (signal == SIGTERM)) {
	close_unlink();
	exit(0);
    }
}


int main(int argc, char *argv[])
{
    signal(SIGINT, quitHandler);
    signal(SIGTERM, quitHandler);
    signal(SIGKILL, quitHandler);

    // validate arguments from command line ------------------------------------
    if (argc < 3) {
	printf("Insufficient arguments.\n");
	return -1;
    }

    else if (strlen(argv[1]) > 5) {
	printf("error: Client ID too long\n");
	return -1;
    }

    else if (is_valid_num(argv[2]) < 0) {
	printf("error: Number of pages is not a positive integer\n");
	return -1;
    }

    // open shared memory segment as if it was a file --------------------------
    shm_fd = shm_open(name, O_RDWR, 0666);
    if (shm_fd == -1) {
	printf("cons: Shared memory failed: %s\n", strerror(errno));
	exit(1);
    }

    // map shared memory segment to address space of process -------------------
    shm_base = (job_control *)mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_base == MAP_FAILED) {
	printf("cons: Map failed: %s\n", strerror(errno));
	close_unlink();
	exit(1);
    }

    // access variables in shared memory ----------------------------------------
    void *ptr = shm_base;
    sem_t *empty = (sem_t *)ptr;

    ptr += sizeof(sem_t);
    sem_t *mutex = (sem_t *)ptr;

    ptr += sizeof(sem_t);
    sem_t *full = (sem_t *)ptr;

    ptr += sizeof(sem_t);
    int slots = *(int *)ptr;

    ptr += sizeof(int);
    int *queue_in = (int *)ptr;

    ptr += sizeof(int);
    int *queue_out = (int *)ptr;

    ptr += sizeof(int);
    job_t *job_queue = (job_t *)ptr;


    // put job in queue ---------------------------------------------------------
    if (sem_trywait(empty) == -1) {
	printf("Client %s has %s pages to print, buffer full, sleeps\n", argv[1], argv[2]);
	sem_wait(empty);
	printf("Client %s wakes up, puts request in Buffer[%d]\n", argv[1], *queue_in);
    }
    else
	printf("Client %s has %s pages to print, puts request in Buffer[%d]\n", argv[1], argv[2], *queue_in);

    sem_wait(mutex);

    int sem_val = 0;
    job_queue += *queue_in * sizeof(job_t);
    memcpy(job_queue->client_ID, argv[1], MAX_ID*sizeof(char));
    sscanf(argv[2], "%d", &(job_queue->pages));
    *queue_in = (*queue_in + 1) % slots;
    sem_getvalue(full, &sem_val);

    sem_post(mutex);
    sem_post(full);


    // release shared memory ----------------------------------------------------
    close_unlink();

    return 0;
}

