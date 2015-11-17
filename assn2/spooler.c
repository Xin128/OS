/* Lara Goxhaj          * 260574574 *
 * COMP 310/ECSE 427    * assn 2    *
 * -------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <errno.h>
#include <signal.h>
#include "jobs.h"


int shm_fd;		// file descriptor

void quitHandler(int signal) {
    if ((signal == SIGINT) || (signal == SIGQUIT) || (signal == SIGTERM)) {

	// close shared memory segment as if it was a file ----------------------
	if (close(shm_fd) == -1) {
	    printf("\ncons: Close failed: %s\n", strerror(errno));
	    exit(1);
	}

	/* remove the shared memory segment from the file system */
	if (shm_unlink(name) == -1) {
	    printf("\ncons: Error removing %s: %s\n", name, strerror(errno));
	    exit(1);
	}
	printf("\nShared memory removed; shutting down printer\n");

	exit(0);
    }
}


int main(int argc, char *argv[])
{
    int queue_size = 10;	// default
    int temp;
    // validate arguments from command line ------------------------------------
    if (argc < 2)
	printf("No slot number specified; using default %d slots\n", queue_size);

    else if (!(sscanf (argv[1], "%i", &temp) == 1))
	printf("error: Number of slots is not an integer; using default %d slots\n", queue_size);

    else {
	queue_size = atoi(argv[1]);
	printf("Using specified %d slots\n", queue_size);
    }

    // arguments ---------------------------------------------------------------
    void *shm_base;	// base address, from mmap()


    // shared memory -----------------------------------------------------------
    shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);	// create shared memory
    ftruncate(shm_fd,SIZE);				// configure size of shared mem segment


    // map shared memory segment in address space of process
    shm_base = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_base == MAP_FAILED) {
	printf("Map failed\n");
	return -1;
    }

    // empty and full counting semaphores, to count # of empty or full buffer slots respectively

    sem_t *empty = (sem_t *)shm_base;
    if (sem_init(empty, 1, queue_size+1) == -1) {
	// empty initialised to N, so waiting on it means waiting if buffer full
	printf("Initialising semaphore empty failed: %s\n", strerror(errno));
	return -1;
    }
    shm_base += sizeof(sem_t);

    sem_t *mutex = (sem_t *)shm_base;
    if (sem_init(mutex, 1, 1) == -1) {
	printf("Initialising semaphore mutex failed: %s\n", strerror(errno));
	return -1;
    }
    shm_base += sizeof(sem_t);

    sem_t *full = (sem_t *)shm_base;
    if (sem_init(full, 1, 0) == -1) {
	// full initialised to 0, so waiting on it means waiting if buffer empty
	printf("Initialising semaphore full failed: %s\n", strerror(errno));
	return -1;
    }
    shm_base += sizeof(sem_t);

    *(int *)shm_base = queue_size;
    int *slots = (int *)shm_base;
    shm_base += sizeof(int);

    *(int *)shm_base = 0;
    int *queue_in = (int *)shm_base;
    shm_base += sizeof(int);

    *(int *)shm_base = 0;
    int *queue_out = (int *)shm_base;
    shm_base += sizeof(int);

    job_t *job_queue = (job_t *)shm_base;


    job_t *job_out;
    int q_out = 0;
    int jobs = 0;
    while(1)
    {
	signal(SIGINT, quitHandler);
	signal(SIGTERM, quitHandler);
	signal(SIGKILL, quitHandler);

	// take a job - blocking on semaphore if no job ------------------------
	// consumer; protects critical section; counts empty slots; counts full buffer slots
	if (sem_trywait(full) == -1) {
	    printf("No request in buffer, printer sleeps\n");
	    sem_wait(full);
	}
	sem_wait(mutex);

	job_out = job_queue + *queue_out * sizeof(job_t);
	jobs++;
	q_out = *queue_out;
	*queue_out = (*queue_out + 1) % *slots;

	// print job message - duration, ID, source ----------------------------
	printf("Printing %d pages from Client %s: Buffer[%d], Job[%d]\n", job_out->pages, job_out->client_ID, q_out, jobs);

	sem_post(mutex);
	sem_post(empty);

	// sleep for job duration - 1 s/page -----------------------------------
	sleep(job_out->pages);
	printf("Done printing %d pages from Client %s: Buffer[%d], Job[%d]\n", job_out->pages, job_out->client_ID, q_out, jobs);

    }
}


