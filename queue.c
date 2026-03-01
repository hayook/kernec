#include "queue.h"
#include "vector.h"
#include <assert.h>
#include <stdlib.h>

struct _Queue {
  Vector *v;
  size_t head;
  size_t tail;
  size_t length;
  size_t element_size;
  size_t init;
};

void queue_validate(Queue *q) {
  assert(q != NULL && q->init && "Invalid queue");
}

Queue *queue_new(size_t element_size) {
  Vector *v = vector_new(element_size);
  if (!v)
    return NULL;

  Queue *q = malloc(sizeof(Queue));
  if (!q) {
    vector_free(v);
    return NULL;
  }

  q->v = v;
  q->head = q->tail = q->length = 0;
  q->element_size = element_size;
  q->init = 1;
  return q;
}

void queue_free(Queue *q) {
  queue_validate(q);

  vector_free(q->v);
  free(q);
}

int queue_append(Queue *q, void *buf) {
  queue_validate(q);

  int err;
  /* Queue is full => the vectorr is full <=> needs resize */
  if (q->length >= vector_capacity(q->v)) {
    /* Realign elements if needed */
    for (size_t i = 0; i < q->head; i++) {
      void *temp = malloc(q->element_size);
	  if (!temp)
		  return -1;
      err = vector_popleft(q->v, temp);
      if (err) {
		  free(temp);
        return -1;
	  }
      err = vector_append(q->v, temp);
	  free(temp);
      if (err)
        return -1;
    }
    q->head = 0;
    /* My current cleverness level says that this is safe */
    q->tail = q->length;
  }

  err = vector_write(q->v, q->tail, buf);
  if (err)
    return -1;

  q->tail = (q->tail + 1) % vector_capacity(q->v);
  q->length++;
  return 0;
}

int queue_pop(Queue *q, void *buf) {
  if (q->length == 0)
    return -1;

  if (buf)
    vector_read(q->v, q->head, buf);

  q->head = (q->head + 1) % vector_capacity(q->v);
  q->length--;
  return 0;
}

size_t queue_length(Queue *q) {
	queue_validate(q);

	return q->length;
}
