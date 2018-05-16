/* 
 *     
 * 
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <error.h>
#include <time.h>

#include "pipe.h"
/*
 *  * new_cb() creates and initializes a new circular buffer.
 *   */
pipe_t *pipe_new(long queue_max)
{
	pipe_t *ptr = (pipe_t *)malloc(sizeof(pipe_t));
	if (ptr == NULL)
	{
		return (NULL);
	}
	memset(ptr, 0, sizeof(pipe_t));
	TAILQ_INIT(&ptr->head);
	ptr->queue_max = queue_max;
	pthread_mutex_init(&ptr->buf_lock, NULL);
	pthread_cond_init(&ptr->notfull, NULL);
	pthread_cond_init(&ptr->notempty, NULL);
	return (ptr);
}

/*
 *  * delete_cb() frees a circular buffer.
 *   */
void pipe_free(pipe_t *ptr)
{
	if (!ptr) return;
	pthread_mutex_lock(&ptr->buf_lock);
	/* wait while there is nothing in the buffer */
	while (!TAILQ_EMPTY(&ptr->head))
	{
		pointer_t *p = TAILQ_FIRST(&ptr->head);
		free(p);
		TAILQ_REMOVE(&ptr->head, p, pointers);
	}
	pthread_mutex_unlock(&ptr->buf_lock);
	pthread_mutex_destroy(&ptr->buf_lock);
	pthread_cond_destroy(&ptr->notfull);
	pthread_cond_destroy(&ptr->notempty);
	free(ptr);
}

/*
 *  * put_cb_data() puts new data on the queue.
 *   * If the queue is full, it waits until there is room.
 *    */
void pipe_pushv(pipe_t *ptr, void **data, int n)
{
	pointer_t *p[n];

	for (int i = 0; i < n; i++)
	{
		p[i] = malloc(sizeof(pointer_t *));
		if (!p[i])
		{
			perror("malloc");
			exit(EXIT_FAILURE);
		}
		p[i]->data = data[i];
	}

	pthread_mutex_lock(&ptr->buf_lock);
	while ((ptr->queue_length + n) >= ptr->queue_max)
	{
		pthread_cond_wait(&ptr->notfull, &ptr->buf_lock);
	}

	for (int i = 0; i < n; i++)
	{
		ptr->queue_length++;
		TAILQ_INSERT_TAIL(&ptr->head, p[i], pointers);
	}
	pthread_cond_signal(&ptr->notempty);
	pthread_mutex_unlock(&ptr->buf_lock);
}

/*
 *  * put_cb_data() puts new data on the queue.
 *   * If the queue is full, it waits until there is room.
 *    */
void pipe_push(pipe_t *ptr, void *data)
{
	pointer_t *p = malloc(sizeof(pointer_t *));
	if (!p)
	{
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	p->data = data;
	pthread_mutex_lock(&ptr->buf_lock);
	while (ptr->queue_length >= ptr->queue_max)
	{
		pthread_cond_wait(&ptr->notfull, &ptr->buf_lock);
	}
	ptr->queue_length++;
	TAILQ_INSERT_TAIL(&ptr->head, p, pointers);
	pthread_cond_signal(&ptr->notempty);
	pthread_mutex_unlock(&ptr->buf_lock);
}

/*
 *  * get_cb_data() gets the oldest data in the circular buffer.
 *   * If there is none, it waits until new data appears.
 *    */
void *pipe_pop(pipe_t *ptr)
{
	void *data = NULL;
	pthread_mutex_lock(&ptr->buf_lock);
	/* wait while there is nothing in the buffer */
	while (TAILQ_EMPTY(&ptr->head))
	{
		pthread_cond_wait(&ptr->notempty, &ptr->buf_lock);
	}
	pointer_t *p = TAILQ_FIRST(&ptr->head);
	TAILQ_REMOVE(&ptr->head, p, pointers);
	data = p->data;
	ptr->queue_length--;
	free(p);
	pthread_cond_signal(&ptr->notfull);
	pthread_mutex_unlock(&ptr->buf_lock);
	return (data);
}

/*
 *  * get_cb_data() gets the oldest data in the circular buffer.
 *   * If there is none, it waits until new data appears.
 *    */
int pipe_popv(pipe_t *ptr, void *data[], int max)
{
	pthread_mutex_lock(&ptr->buf_lock);
	/* wait while there is nothing in the buffer */
	while (TAILQ_EMPTY(&ptr->head))
	{
		pthread_cond_wait(&ptr->notempty, &ptr->buf_lock);
	}
	int n = 0;
	int i = 0;
	for (i = 0; i < max; i++)
	{
		pointer_t *p = TAILQ_FIRST(&ptr->head);
		data[i] = p->data;
		ptr->queue_length--;
		TAILQ_REMOVE(&ptr->head, p, pointers);
		free(p);
		n++;
		if (TAILQ_EMPTY(&ptr->head))
			break;
	}
	pthread_cond_signal(&ptr->notfull);
	pthread_mutex_unlock(&ptr->buf_lock);
	return n;
}
