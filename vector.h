#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

typedef struct _Vector Vector;

#define INIT_CAP 1024

Vector *vector_new(size_t element_size);
void vector_free(Vector *v);
size_t vector_length(Vector *v);
size_t vector_capacity(Vector *v);
int vector_read(const Vector *v, size_t pos, void *buf);
int vector_write(Vector *v, size_t pos, void *buf);
int vector_remove(Vector *v, size_t pos, void *buf);
int vector_append(Vector *v, void *buf);
int vector_pop(Vector *v, void *buf);
int vector_popleft(Vector *v, void *buf);

#endif
