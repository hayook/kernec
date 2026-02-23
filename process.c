#include "process.h"
#include <assert.h>
#include <stdlib.h>

size_t pid = 0;

Process create_process(const block code[], size_t code_size) {
  Process p = {0};
  p.pid = pid;
  p.code = code;
  p.code_size = code_size;
  pid++;
  return p;
}

/* Input and output of this function are immutable anyway,
 * the only reason i am using a pointer is to avoid copying.
 * TODO: That should be applied to other functions as well.
 */
const block *get_current_block(const Process *p) {
  /* Returns a pointer to the current block or NULL if finished. */

  /* There is no way this can happen, since the OS will remove the process
   * once it reaches the end by generating exit syscall.
   */
  assert(p->ip.index <= p->code_size &&
         "Unreachable: Segmentation fault (core dumped).");
  if (p->ip.index < p->code_size)
    return &p->code[p->ip.index];

  return NULL;
}

/*
 * I think it's much easier and simpler to split advance_block() and
 * advance_offset(), rather than having them in one single function, at least
 * for now.
 */
void advance_block(Process *p) {
  if (!p)
    return;

  p->ip.index++;
  p->ip.offset = 0;
}

void advance_offset(Process *p, size_t off) {
  /* TODO: Some code block types operations currently doesn't have cost, so
   *       that's dangerous.
   */
  if (!p || off <= p->ip.offset || off > p->code->cost)
    return;

  p->ip.offset = off;
}
