#include "kernec.h"
#include "process.h"
#include "queue.h"
#include <assert.h>
#include <stdio.h>

/* TODO: Unhard-code programs code */
const block p1_code[] = {{.op = OP_CPU, .cost = 0.27},
                         {.op = OP_CPU, .cost = 0.27}};

queue ready_queue = {0};

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
   * create a new process, we replace the current porcess, we just modify the
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
  int ret = queue_append(&ready_queue, p);
  if (ret != 0)
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
  if (!running_process)
    assert(!"Unreachable: OS should be terminated at this point since we don't "
            "have IO");

  for (;;) {
    const block *b = get_current_block(&current);
    current.ip.index++;

    if (b == NULL) {
      /* Process termination */
      break;
    } else if (b->op == OP_CPU) {
      clock += b->cost;
    }
  }
}

int main(void) {
  int ret;

  /* Kernel init
   * Note: To be more realistic, kernel init should also consume simulation time
   * but we'll see, for simplicity, currently kernel code time is negligible
   */

  /* Load  programs into the system
   * Note: In a real scenario, this would happen at different times. for now, we
   * load them all at the beginning.
   */
  ret = load_program("Web Browser", p1_code,
                     sizeof(p1_code) / sizeof(p1_code[0]));
  if (ret != 0) {
    printf("Can't load program\n");
    return 1;
  }

  ret = load_program("Web Browser", p1_code,
                     sizeof(p1_code) / sizeof(p1_code[0]));
  if (ret != 0) {
    printf("Can't load program\n");
    return 1;
  }

  ret = queue_pop(&ready_queue, &current);
  if (ret == 0) {
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

    /* Handle exit syscall
     * Currently, the only way kernel will gain access back to the CPU is when
     * the process make an exit syscall, now, the exit syscall it's inevitable
     * when the process reaches the end of the code, if next_event() doesn't
     * return at that point, a segmentation fault will be generated (assuming
     * that next_event() will break only in that case), i have a better plan for
     * the interrupt model by using an interrupt events queue, but for now, this
     * is a simple way to do it.
     * Long story short: next_event() should return when the end of code is
     * reached, if it didn't the process will seg fault, when it returns the
     * kernel will handle the termination since it's the only possible interrupt
     * for now.
     */
    printf("[ OS ]  process %lu exited at %f.\n", current.pid, clock);
    running_process = 0;
    ret = queue_pop(&ready_queue, &current);
    if (ret == 0) {
      running_process = 1;
      start_process(current);
    }
  }

  return 0;
}
