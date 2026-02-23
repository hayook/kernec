#ifndef QUEUE_H
#define QUEUE_H

#include "process.h"

/* Queue (Fixed array) for processes.
 * TODO: Create a generic dynamic array regardless of items type.
 */

#define QUEUE_SZ 64
typedef struct {
  Process items[QUEUE_SZ];
  int head;
  int tail;
  int length;
} queue;

int queue_append(queue *q, Process p);

int queue_pop(queue *q, Process *p);


#endif
