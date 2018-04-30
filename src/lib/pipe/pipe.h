#ifndef _PIPE_H_
#define _PIPE_H_

#include <sys/queue.h>

typedef struct pointer
{
	void *data;
	TAILQ_ENTRY(pointer) pointers;
} pointer_t;

typedef struct
{
	long queue_max;
	volatile long queue_length;
	pthread_mutex_t buf_lock; /* lock the structure */
	pthread_cond_t notfull;   /* full -> not full condition */
	pthread_cond_t notempty;  /* empty -> not empty condition */
	TAILQ_HEAD(tailhead, pointer) head;
} pipe_t;

pipe_t *pipe_new(long queue_length);
void pipe_free(pipe_t *ptr);
void pipe_push(pipe_t *ptr, void *data);
void *pipe_pop(pipe_t *ptr);
int pipe_popv(pipe_t *ptr, void *data[], int max);
void pipe_pushv(pipe_t *ptr, void *data[], int n);
#endif

