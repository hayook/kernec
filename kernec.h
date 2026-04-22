#ifndef KERNEC_H
#define KERNEC_H

#include <stdlib.h>

#define NAME_BUF 256

enum BlockOperations {
  OP_CPU,
};

typedef struct {
  enum BlockOperations op;
  double cost;
  size_t dur;
} block;

typedef struct {
	char name[NAME_BUF];
	block *code;
	size_t code_size;
} Program;

enum InterruptEvents {
  INT_EXIT,
};

typedef struct {
  enum InterruptEvents type;
  double at;
} Event;

#endif
