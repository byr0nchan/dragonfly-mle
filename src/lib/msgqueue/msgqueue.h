#ifndef _MSG_QUEUE_H_
#define _MSG_QUEUE_H_

#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

#define MAX_QUEUE_MSG_SIZE	4096

typedef struct
{
	volatile int cancel;
	mqd_t mq;
	char *queue_name;
	struct mq_attr attr;
} queue_t;

queue_t *msgqueue_create(const char *queue_name, int msg_max, long queue_max);
queue_t *msgqueue_open(const char *queue_name);
void msgqueue_cancel(queue_t *q);
int msgqueue_send(queue_t *q, const char *buffer, int length);
int msgqueue_recv(queue_t *q, char *buffer, int max_size);
void msgqueue_close(queue_t *q);
void msgqueue_destroy(queue_t *q);

#endif
