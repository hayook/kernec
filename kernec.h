#ifndef KERNEC_H
#define KERNEC_H

#include <stdlib.h>

enum BlockOperations {
	OP_CPU,
};

typedef struct {
  enum BlockOperations op;
  double cost;
  size_t dur;
} block;

#endif
