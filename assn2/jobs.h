#ifndef JOBS_H_
#define JOBS_H_

#define MAX_ID 3

typedef struct job_t
{
    char client_ID[MAX_ID];
    int pages;
} job_t;

typedef struct job_control
{
    sem_t empty;
    sem_t mutex;
    sem_t full;
    int slots;
    int queue_in;
    int queue_out;
    job_t *job_queue;
} job_control;

// shared memory file
const char *name = "/shm-assn2";	// file name
const int SIZE = 4096;			// file size


#endif
