#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>

typedef struct _Queue Queue;

Queue *queue_new(size_t element_size);
void queue_free(Queue *q);
int queue_append(Queue *q, void *buf);
int queue_pop(Queue *q, void *buf);
size_t queue_length(Queue *q);

#endif
