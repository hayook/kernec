#include "kernec.h"
#include "process.h"
#include "queue.h"
#include <assert.h>
#include <stdio.h>

/* TODO: Unhard-code programs code */
const block p1_code[] = {{.op = OP_CPU, .cost = 0.27},
                         {.op = OP_CPU, .cost = 0.27}};

Queue *ready_queue;
Queue *interrupts_queue;

double clock = 0;

/* Note: I need a reliable indicator to tell whether there is a current
 * process or no, either `current` be a pointer, another variable, for now i'll
 * go with another variable, i don't know whether there are better options.
 * (Maybe current.state? but `current` will eventually be a global pointer)
 */
Process current = {0};
size_t running_process = 0;

int load_program(const char *name, const block code[], int code_size) {
  /* Loads a program into the system
   * Note: In reality, this should be in exec handler, also we don't actually
   * create a new process, we replace the current process, we just modify the
   * memory and some structure attributes, but since we don't have an init
   * system, or fork, or anything like that, and given the fact that programs
   * are loaded at the beginning anyways, we can't implement such things for
   * now.
   * If it's convenient and easy to get as close as possible to real kernels,
   * that's fine, otherwise, that's just a mechanism kernels chose, i can choose
   * any other mechanism works well.
   */

  /* Create a container for the program, that will hold the address
   * where the program is stored, in memory
   */
  Process p = create_process(code, code_size);

  /* Note: I don't know whether i need a list to store all the processes, or
   * just store processes directly in the ready queue, i will introduce it when
   * i really need it, also i don't know whether Linux has a list for all
   * processes, or they are spread across different queues.
   */
  int err = queue_append(ready_queue, &p);
  if (err)
    return -1;

  printf("[ OS ] Loading program '%s'...\n", name);
  printf("[ OS ] Process %lu created.\n", p.pid);
  printf("[ Scheduler ] Process %lu added to the ready queue\n", p.pid);
  return 0;
}

void start_process(Process p) {
  printf("[ OS ]  CPU will be assigned to process %lu\n", p.pid);
}

void next_event() {
  int err;
  Event ev = {0};

  if (!running_process)
    assert(!"Unreachable: OS should be terminated at this point since we don't "
            "have IO");

  for (;;) {
    const block *b = get_current_block(&current);
    advance_block(&current);

    if (b == NULL) {
      /* Process termination */
      ev.type = INT_EXIT;
      ev.at = clock;
      err = queue_append(interrupts_queue, &ev);
      /* TODO: I thought next_event won't failt, it should return error code, or
       * should it?
       */
      assert(!err && "next_event cannot append event");
      break;
    } else if (b->op == OP_CPU) {
      clock += b->cost;
    }
  }
}

int main(void) {
  int ret = 1;
  int err;

  ready_queue = queue_new(sizeof(Process));

  interrupts_queue = queue_new(sizeof(Event));
  if (!interrupts_queue) {
    printf("ERROR: failed to create the events queueu\n");
    goto out_free;
  }

  /* Kernel init
   * Note: To be more realistic, kernel init should also consume simulation time
   * but we'll see, for simplicity, currently kernel code time is negligible
   */

  /* Load  programs into the system
   * Note: In a real scenario, this would happen at different times. for now, we
   * load them all at the beginning.
   */
  err = load_program("Web Browser", p1_code,
                     sizeof(p1_code) / sizeof(p1_code[0]));
  if (err) {
    printf("Can't load program\n");
    goto out_free;
  }

  err = load_program("Web Browser", p1_code,
                     sizeof(p1_code) / sizeof(p1_code[0]));
  if (err) {
    printf("Can't load program\n");
    goto out_free;
    return 1;
  }

  err = queue_pop(ready_queue, &current);
  if (!err) {
    running_process = 1;
    start_process(current);
  }

  /* After running the init process (which is not really init in this case, just
   * a process at first) the kernel will stay idle in memory waiting for
   * interrupts (syscalls and hardware interrupts).
   */
  while (1) {
    /* Return to user
     * I think it would be better to have some shared logic between handlers,
     * for example check scheduling is needed everytime before you return to
     * user mode, anytime the kernel receives the CPU it should exploit this
     * opportunity.
     */

    /* Termination conditions */
    if (!running_process) {
      printf("[ OS ]  Terminated\n");
      break;
    }

    printf("--- CPU: Switched to user mode ---\n");
    next_event();
    printf("\n--- CPU: Switched to kernel mode ---\n");

    while (queue_length(interrupts_queue) > 0) {
      Event ev;
      err = queue_pop(interrupts_queue, &ev);
      if (err)
        goto out_free;

      switch (ev.type) {
      case INT_EXIT:
        printf("[ OS ]  process %lu exited at %f.\n", current.pid, ev.at);
        running_process = 0;
        err = queue_pop(ready_queue, &current);
        /* this error doesn't mean the queue is empty, and if it's not empty and
         * there is an error, i don't know what is the right thing to do in this
         * case, maybe check for whether the queue is empty using the
         * queue_length() or special error codes when introduced, not sure,
         * we'll see.*/
        if (!err) {
          running_process = 1;
          start_process(current);
        }
        break;
      }
    }
  }

  ret = 0;
out_free:
  /* TODO: Improve the cleanup model
   * If the function jumps here to free a pointer, before it allocates its
   * memory, something bad will happen, i cannot free all pointers at once, or
   * find a better way..
   */
  queue_free(ready_queue);
  queue_free(interrupts_queue);
  return ret;
}
