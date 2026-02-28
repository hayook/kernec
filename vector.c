#include "vector.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Constraint: Elements should stay compact (No garbage in between).
 * Otherwise i would either have space waste, or have to implement a
 * logic to keep track the empty spots. therefor shifting is required.
 */

struct _Vector {
  void *items;
  size_t length;
  size_t capacity;
  size_t element_size;
  int init;
};

void vector_validate(const Vector *v) {
  assert(v != NULL && v->init && "Invalid vector");
}

/* TODO: Improve error codes */

Vector *vector_new(size_t element_size) {
  if (!element_size)
    return NULL;

  Vector *v = malloc(sizeof(Vector));
  if (!v)
    return NULL;

  void *result = malloc(INIT_CAP);
  if (!result) {
    free(v);
    return NULL;
  }

  v->items = result;
  v->length = 0;
  v->capacity = INIT_CAP / element_size;
  v->element_size = element_size;
  v->init = 1;

  return v;
}

void vector_free(Vector *v) {
  vector_validate(v);

  assert(v->items != NULL && "Unreachable: `init` is not working");
  free(v->items);
  free(v);
}

size_t vector_length(Vector *v) {
  vector_validate(v);

  return v->length;
}

size_t vector_capacity(Vector *v) {
  vector_validate(v);

  return v->capacity;
}

int vector_read(const Vector *v, size_t pos, void *buf) {
  vector_validate(v);

  if (pos >= v->length || !buf)
    return -1;

  memcpy(buf, (unsigned char *)v->items + (pos * v->element_size),
         v->element_size);

  return 0;
}

int vector_write(Vector *v, size_t pos, void *buf) {
  vector_validate(v);

  if (pos > v->length || !buf)
    return -1;

  const size_t size = v->element_size;
  /* If this write operation is an append, we need to check for resize */
  if (pos == v->length) {
    if (v->length >= v->capacity) {
      void *result = malloc(v->capacity * 2 * size);
      if (!result)
        return -1;

      /* copy data */
      memcpy(result, v->items, v->length * size);
      free(v->items);
      v->items = result;
      v->capacity *= 2;
    }
    v->length += 1;
  }

  memcpy((unsigned char *)v->items + (size * pos), buf, size);

  return 0;
}

int vector_remove(Vector *v, size_t pos, void *buf) {
  vector_validate(v);

  if (pos >= v->length)
    return -1;

  const size_t size = v->element_size;
  if (buf)
    memcpy(buf, (unsigned char *)v->items + (pos * size), size);

  /* Shift elements */
  if (pos + 1 != v->length)
    memcpy((unsigned char *)v->items + (pos * size),
           (unsigned char *)v->items + ((pos + 1) * size),
           (v->length - pos - 1) * size);

  v->length -= 1;

  return 0;
}

int vector_append(Vector *v, void *buf) {
  return vector_write(v, v->length, buf);
}

int vector_pop(Vector *v, void *buf) {
  vector_validate(v);

  if (v->length == 0)
    return -1;

  return vector_remove(v, v->length - 1, buf);
}

int vector_popleft(Vector *v, void *buf) {
  int retval = vector_remove(v, 0, buf);
  return retval;
}
