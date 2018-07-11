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
#include <fcntl.h>
#include <sys/uio.h>

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
#ifdef __DEBUG3__
	fprintf(stderr, "%s:%i\n", __FUNCTION__, __LINE__);
#endif
#ifdef COMMENT_OUT
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
	mqd_t mq = mq_open(queue_name, O_CLOEXEC | O_RDWR, 0644, &attr);
	if (mq > 0)
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
		mq_close(mq);
		mq_unlink(queue_name);
	}
#endif
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
queue_t *msgqueue_create(const char *queue_name, int msg_max, long queue_max)
{
#ifdef __DEBUG3__
	fprintf(stderr, "%s:%i\n", __FUNCTION__, __LINE__);
#endif
	queue_t *q = (queue_t *)malloc(sizeof(queue_t));
	if (!q)
		return NULL;

	memset(q, 0, sizeof(queue_t));
	q->queue_name = strndup(queue_name, NAME_MAX);
	if (pipe(q->pipefd) < 0)
	{
		syslog(LOG_ERR, "pipe() error: %s", strerror(errno));
		fprintf(stderr, "pipe() error: %s", strerror(errno));
		free(q);
		exit(EXIT_FAILURE);
	}
	int flags = fcntl(q->pipefd[0], F_GETFD);
	flags |= O_NONBLOCK;
	if (fcntl(q->pipefd[0], F_SETFD, flags))
	{
		syslog(LOG_ERR, "pipe() F_SETFD error: %s", strerror(errno));
		fprintf(stderr, "pipe() F_SETFD error: %s", strerror(errno));
		free(q);
		exit(EXIT_FAILURE);
	}
	flags = fcntl(q->pipefd[1], F_GETFD);
	flags |= O_NONBLOCK;
	if (fcntl(q->pipefd[1], F_SETFD, flags))
	{
		syslog(LOG_ERR, "pipe() F_SETFD error: %s", strerror(errno));
		fprintf(stderr, "pipe() F_SETFD error: %s", strerror(errno));
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
#ifdef __DEBUG3__
	fprintf(stderr, "%s:%i\n", __FUNCTION__, __LINE__);
#endif
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
#ifdef __DEBUG3__
	fprintf(stderr, "%s:%i\n", __FUNCTION__, __LINE__);
#endif
	if (!q)
		return;
	close(q->pipefd[0]);
	close(q->pipefd[1]);
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
#ifdef __DEBUG3__
	fprintf(stderr, "%s:%i\n", __FUNCTION__, __LINE__);
#endif
	if (!q || q->cancel)
		return -1;

	if (length > MAX_MESSAGE_SIZE)
	{
		length = MAX_MESSAGE_SIZE;
	}
	int n = 0;

	struct iovec iov[2];
	iov[0].iov_base = &length;
	iov[0].iov_len = sizeof(length);
	iov[1].iov_base = (void *)buffer;
	iov[1].iov_len = length;
	do
	{
		//fprintf(stderr, "%s: write() length: - %i\n", __FUNCTION__, length);
		n = writev(q->pipefd[1], iov, 2);
		if (n <= 0)
		{
			if (n == 0)
				return 0;
#ifdef __DEBUG3__
			fprintf(stderr, "%s: write() error: - %s\n", __FUNCTION__, strerror(errno));
#endif
			switch (errno)
			{
			case ETIMEDOUT:
			case EWOULDBLOCK:
				//fprintf(stderr, "%s: EAGAIN(%i)\n", __FUNCTION__, errno);
				usleep(QUANTUM_SLEEP);
				break;
			default:
				if (!q->cancel)
				{
					fprintf(stderr, "%s: error: %d - %s\n", __FUNCTION__, errno, strerror(errno));
					syslog(LOG_ERR, "%s: error: %d - %s", __FUNCTION__, errno, strerror(errno));
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
#ifdef __DEBUG3__
	fprintf(stderr, "%s:%i\n", __FUNCTION__, __LINE__);
#endif
	if (!q || q->cancel)
		return -1;

	if (max_size > MAX_MESSAGE_SIZE)
	{
		max_size = MAX_MESSAGE_SIZE;
	}
	memset(buffer, 0, max_size);
	int n = 0;
	int l = -1;
	do
	{
		if (l < 0)
		{
			n = read(q->pipefd[0], &l, sizeof(int));
			if (n <= 0)
			{
				if (n == 0)
					return 0;
#ifdef __DEBUG3__
				fprintf(stderr, "%s: read() error: %s\n", __FUNCTION__, strerror(errno));
#endif
				switch (errno)
				{
				case ETIMEDOUT:
				case EAGAIN:
					//fprintf(stderr, "%s: EAGAIN(%i)\n", __FUNCTION__, errno);
					usleep(QUANTUM_SLEEP);
					break;
				default:
					if (!q->cancel)
					{
						fprintf(stderr, "%s: error: %d - %s\n", __FUNCTION__, errno, strerror(errno));
						syslog(LOG_ERR, "%s: error: %d - %s", __FUNCTION__, errno, strerror(errno));
						;
						exit(EXIT_FAILURE);
					}
					break;
				}
			}
			//fprintf(stderr, "%s:recv length = %i\n", __FUNCTION__, l);
			// need a better way to handle this
			if (l > max_size)
			{
				fprintf(stderr, "%s: error: message bigger than buffer size\n", __FUNCTION__);
				syslog(LOG_ERR, "%s: error: message bigger than buffer size", __FUNCTION__);
				exit(EXIT_FAILURE);
			}
			if (q->cancel)
				return 0;
			n = read(q->pipefd[0], buffer, l);
			if (n <= 0)
			{
				if (n == 0)
					return 0;
#ifdef __DEBUG__
				fprintf(stderr, "%s: read() error: %s\n", __FUNCTION__, strerror(errno));
#endif
				switch (errno)
				{
				case ETIMEDOUT:
				case EAGAIN:
					//fprintf(stderr, "%s: EAGAIN(%i)\n", __FUNCTION__, errno);
					usleep(QUANTUM_SLEEP);
					break;
				default:
					if (!q->cancel)
					{
						fprintf(stderr, "%s: error: %d - %s\n", __FUNCTION__, errno, strerror(errno));
						syslog(LOG_ERR, "%s: error: %d - %s", __FUNCTION__, errno, strerror(errno));

						exit(EXIT_FAILURE);
					}
					break;
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
