/*
 * ---------------------------------------------------------------------------------------
 *
 * 
 * ---------------------------------------------------------------------------------------
 */
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <syslog.h>

#include <sys/resource.h>

#ifdef __linux__
#include <error.h>
#endif

#include "msgqueue.h"

#define QUANTUM	500000
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
queue_t *msgqueue_create(const char *queue_name, int msg_max, long queue_max)
{
	queue_t *q = (queue_t *)malloc(sizeof(queue_t));
	if (!q)
		return NULL;

	struct rlimit rlim;
	rlim.rlim_cur = RLIM_INFINITY;
	rlim.rlim_max = RLIM_INFINITY;

	if (setrlimit(RLIMIT_MSGQUEUE, &rlim) == -1)
	{
		perror("setrlimit");
		return NULL;
	}

	memset(q, 0, sizeof(queue_t));
	q->queue_name = strndup(queue_name, NAME_MAX);
	q->attr.mq_maxmsg = queue_max;
	q->attr.mq_msgsize = msg_max;
	q->attr.mq_curmsgs = 0;

	/* create the message queue */
	q->mq = mq_open(q->queue_name, O_CREAT | O_CLOEXEC | O_RDWR, 0644, &q->attr);
	if (q->mq < 0)
	{
		syslog(LOG_ERR, "mq_open() error: %s", strerror(errno));
		free(q);
		exit(EXIT_FAILURE);
	}
	return q;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void msgqueue_cancel(queue_t *q)
{
	if (!q || q->cancel)
		return;
	q->cancel = 1;
	struct timespec tmo = {0, QUANTUM };
	nanosleep(&tmo, NULL);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void msgqueue_destroy(queue_t *q)
{
	if (!q)
		return;
	int n = 0;
	struct timespec tm;
	char buffer[4096];
	do
	{
		clock_gettime(CLOCK_REALTIME, &tm);
		tm.tv_nsec += QUANTUM;
		n = mq_timedreceive(q->mq, buffer, sizeof(buffer), NULL, &tm);
	} while (n > 0);

	mq_close(q->mq);
	// TODO:  need to drain the queue
	//mq_unlink(q->queue_name);
	free(q->queue_name);
	free(q);
	q = NULL;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int msgqueue_send(queue_t *q, const char *buffer, int length)
{

	if (!q || q->cancel)
		return -1;
	
	int n = 0;
	struct timespec tm;

	if (length > MAX_QUEUE_MSG_SIZE)
	{
		length = MAX_QUEUE_MSG_SIZE;
	}

	do
	{
		clock_gettime(CLOCK_REALTIME, &tm);
		tm.tv_nsec += QUANTUM;

		n = mq_timedsend(q->mq, buffer, length, 1, &tm);
		if (n < 0)
		{
			switch (errno)
			{
			case ETIMEDOUT:
				break;
			case ENOBUFS:
				break;
			case EAGAIN:
				break;
			case EINVAL:
				break;
			default:
				syslog(LOG_ERR, "mq_timedsend() error: %d - %s", errno, strerror(errno));
				return -1;
			}
		}
	} while (!q->cancel && (n < 0));
	return n;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int msgqueue_recv(queue_t *q, char *buffer, int max_size)
{
	if (!q || q->cancel)
		return -1;

	int n = 0;
	struct timespec tm;
	if (max_size > q->attr.mq_msgsize)
	{
		max_size = q->attr.mq_msgsize;
	}
	memset(buffer, 0, max_size);

	do
	{
		clock_gettime(CLOCK_REALTIME, &tm);
		tm.tv_nsec += QUANTUM;
		n = mq_timedreceive(q->mq, buffer, max_size, NULL, &tm);
		if (n < 0)
		{
			switch (errno)
			{
			case ETIMEDOUT:
				break;
			case ENOBUFS:
				break;
			case EAGAIN:
				break;
			case EINVAL:
				break;
			default:
				syslog(LOG_ERR, "mq_timedreceive() error: %d - %s", errno, strerror(errno));
				return -1;
			}
		}
	} while (!q->cancel && (n < 0));
	return n;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
