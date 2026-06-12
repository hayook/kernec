#ifndef PROCESS_H
#define PROCESS_H

#include <stdlib.h>
#include "kernec.h"

typedef struct {
  size_t index;
  double offset;
} IP;

/* TODO: Add other attributes when needed e.g. instruction pointer, state,
 *       memory, io finish etc
 */
typedef struct {
  size_t pid;
  const block *code;
  size_t code_size;
  IP ip;
} Process;

Process create_process(const block code[], size_t code_size);

const block *get_current_block(const Process *p);

void advance_block(Process *p);

void advance_offset(Process *p, double off);

#endif
