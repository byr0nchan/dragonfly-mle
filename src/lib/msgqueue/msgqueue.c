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

#define QUANTUM 50000
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void msgqueue_reset(const char *queue_name, int msg_max, long queue_max)
{
	char buffer[4096];
	char reset_msg[1024];
	struct timespec tm;
	snprintf(reset_msg, sizeof(reset_msg), "%s-###################", queue_name);
	const int length = strlen(reset_msg);

	struct mq_attr attr;
	attr.mq_maxmsg = queue_max;
	attr.mq_msgsize = msg_max;
	attr.mq_curmsgs = 0;

	/* create the message queue */
	mqd_t mq = mq_open(queue_name, O_RDWR, 0644, &attr);
	if (mq >= 0)
	{
		/*
	 	 *
	     */
		clock_gettime(CLOCK_REALTIME, &tm);
		tm.tv_sec += 2;
		int n = mq_timedsend(mq, reset_msg, length, 1, &tm);
		if (n < 0)
		{
			if (errno != ETIMEDOUT)
			{
				syslog(LOG_ERR, "mq_timedsend() error: %d - %s", errno, strerror(errno));
				fprintf(stderr, "mq_timedsend() error: %d - %s\n", errno, strerror(errno));
			}
			mq_close(mq);
			return;
		}
		/*
		 *
		 */
		do
		{
			n = mq_receive(mq, buffer, msg_max, NULL);
			if (n < 0)
			{
				syslog(LOG_ERR, "mq_receive() error: %d - %s", errno, strerror(errno));
				fprintf(stderr, "mq_receive() error: %d - %s\n", errno, strerror(errno));
				mq_close(mq);
				return;
			}
			if (strncmp(buffer, reset_msg, length) == 0)
			{
#ifdef __DEBUG3__
				fprintf(stderr, "%s: %s success\n", queue_name, __FUNCTION__);
#endif
				break;
			}
		} while (1);
	}
	mq_close(mq);
}

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
		fprintf(stderr, "mq_open() error: %s", strerror(errno));

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
	usleep(25000);
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
	msgqueue_cancel(q);
	do
	{
		clock_gettime(CLOCK_REALTIME, &tm);
		tm.tv_nsec += QUANTUM;
		n = mq_timedreceive(q->mq, buffer, sizeof(buffer), NULL, &tm);
	} while (n > 0);

	mq_close(q->mq);
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
		tm.tv_sec++;
		tm.tv_nsec = 0;
		n = mq_timedsend(q->mq, buffer, length, 1, &tm);
		if (n < 0)
		{
			switch (errno)
			{
			case ETIMEDOUT:
				//fprintf(stderr, "%s: ETIMEDOUT(%i)\n", __FUNCTION__, errno);
				break;
			case EAGAIN:
				//fprintf(stderr, "%s: EAGAIN(%i)\n", __FUNCTION__, errno);
				break;
			case ENOBUFS:
			case EINVAL:
			case EBADF:
			default:
				if (!q->cancel)
				{
					fprintf(stderr, "mq_timedsend() error: %d - %s\n", errno, strerror(errno));
					syslog(LOG_ERR, "mq_timedsend() error: %d - %s", errno, strerror(errno));
					exit(EXIT_FAILURE);
				}
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
		tm.tv_sec++;
		tm.tv_nsec = 0;
		n = mq_timedreceive(q->mq, buffer, max_size, NULL, &tm);
		if (n < 0)
		{
			switch (errno)
			{
			case ETIMEDOUT:
				//fprintf(stderr, "%s: ETIMEDOUT(%i)\n", __FUNCTION__, errno);
				break;
			case EAGAIN:
				//fprintf(stderr, "%s: EAGAIN(%i)\n", __FUNCTION__, errno);
				break;
			case ENOBUFS:
			case EINVAL:
			case EBADF:
			default:
				if (!q->cancel)
				{
					fprintf(stderr, "mq_timedreceive() error: %d - %s\n", errno, strerror(errno));
					syslog(LOG_ERR, "mq_timedreceive() error: %d - %s", errno, strerror(errno));
					exit(EXIT_FAILURE);
				}
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
