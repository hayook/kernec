#include "process.h"
#include "queue.h"

int queue_append(queue *q, Process p) {
  if (q->length >= QUEUE_SZ)
    return -1;

  q->items[q->tail] = p;
  q->length++;
  q->tail = (q->tail + 1) % QUEUE_SZ;
  return 0;
}

int queue_pop(queue *q, Process *p) {
  if (q->length == 0)
    return -1;

  *p = q->items[q->head];
  q->length--;
  q->head = (q->head + 1) % QUEUE_SZ;
  return 0;
}
