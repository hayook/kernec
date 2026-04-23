#include "kernec.h"
#include "klua.h"
#include "process.h"
#include "queue.h"
#include "vector.h"
#include <assert.h>
#include <lua5.3/lauxlib.h>
#include <lua5.3/lua.h>
#include <lua5.3/lualib.h>
#include <stdio.h>
#include <string.h>

#define KLUA_SCRIPT "./init.lua"

/* It's not the best way to manage references and event callbacks, but it's the
 * simplest way for now. And i don't know whther the references created by Lua
 * are guaranteed to be none negative, but we'll find a better indator to tell
 * whether a callback has been registered for a specific event or not.
 */
int exit_cb = -1;
int create_cb = -1;
int init_cb = -1;

Vector *procs_vec;

int current = -1;

Queue *interrupts_queue;

double clock = 0;

int load_program(lua_State *klua_state) {
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

  Program pgm;
  if (klua_parseprogram(klua_state, &pgm) != 0) {
    return 1;
  }

  /* Create a container for the program, that will hold the address
   * where the program is stored, in memory
   */
  Process p = create_process(pgm.code, pgm.code_size);

  if (vector_append(procs_vec, &p))
    return -1;

  printf("[ OS ] Loading program '%s'...\n", pgm.name);
  printf("[ OS ] Process %lu created.\n", p.pid);

  /* Note: There is a difference between these three events:
   * "onExec": exec syscall, you will load the program and create the process.
   * "onLoad": the program is loaded, you will only create the process.
   * "onCreate": the process is created, what to do next.
   */
  if (create_cb != -1) {
    lua_rawgeti(klua_state, LUA_REGISTRYINDEX, create_cb);
    klua_pushprocess(klua_state, vector_length(procs_vec) - 1);
    lua_callk(klua_state, 1, 0, 0, NULL);
  }

  return 0;
}

void start_process(Process p) {
  printf("[ OS ]  CPU will be assigned to process %lu\n", p.pid);
}

void next_event() {
  Process proc;
  int err;
  Event ev = {0};

  if (current == -1)
    assert(!"Unreachable: OS should be terminated at this point since we don't "
            "have IO");

  vector_read(procs_vec, current, &proc);
  for (;;) {
    const block *b = get_current_block(&proc);
    advance_block(&proc);

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
  vector_write(procs_vec, current, &proc);
}

int main(void) {
  int ret = 1;
  int err;
  Process proc;

  procs_vec = vector_new(sizeof(Process));

  interrupts_queue = queue_new(sizeof(Event));
  if (!interrupts_queue) {
    printf("ERROR: failed to create the events queueu\n");
    goto out_free;
  }

  /* Lua init */
  lua_State *klua_state = luaL_newstate();
  luaL_openlibs(klua_state);
  lua_register(klua_state, "onEvent", klua_onEvent);
  lua_register(klua_state, "setCurrent", klua_setCurrent);
  /* TODO: Catch and handle Lua errors and panics */
  luaL_dofile(klua_state, KLUA_SCRIPT);

  /* Kernel init
   * Note: To be more realistic, kernel init should also consume simulation time
   * but we'll see, for simplicity, currently kernel code time is negligible
   */

  /* Load  programs into the system
   * Note: In a real scenario, this would happen at different times. for now, we
   * load them all at the beginning.
   */
  lua_getglobal(klua_state, "job");
  if (lua_istable(klua_state, -1)) {
    size_t job_len = lua_rawlen(klua_state, -1);
    for (int i = 1; i <= job_len; i++) {
      lua_geti(klua_state, -1, i);
      err = load_program(klua_state);
      if (err) {
        printf("Can't load program\n");
        goto out_free;
      }
      lua_pop(klua_state, 1);
    }
  }
  lua_pop(klua_state, 1);

  /* Note: How the user will perform this initial scheduling?
   * I wouldn't need an "initial" scheduling if i had "onExec" event, but i
   * think i can perform it even in onLoad/onCreate events, but for now i'll add
   * onInit event, and we'll try and see whether scheduling onCreate/onLoad will
   * cause issues.
   */
  if (init_cb != -1) {
    lua_rawgeti(klua_state, LUA_REGISTRYINDEX, init_cb);
    lua_callk(klua_state, 0, 0, 0, NULL);
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
    if (current == -1) {
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
        vector_read(procs_vec, current, &proc);
        printf("[ OS ]  process %lu exited at %f.\n", proc.pid, ev.at);
        /* Note: I think let the runtime sets current to false at exit
         * is a good idea, how convenient is it? I think the runtime doesn't
         * care, when a process terminates, current sets it to false as if it's
         * the startup state, i'll keep it in the runtime, if the user scripts
         * need the control it to implement a specific logic we'll see.
         *
         * If the process terminates and the user tries to reset it as the
         * current the process will segfault, so `current = -1` here is
         * not enough to prevent the user, i don't know whther i want to prevent
         * the user from doing such thing or no, since they're responsible for
         * the kernel logic. But anyways, if i want to prevent thakt, the
         * process should be removed from the system, either by removing it from
         * procs_vec (The vector will shift the elements, so the pptrs will be
         * corrupted) or add a process state and prevent setting "terminated"
         * processes as current in setCurrent, we'll see. And of course i'll do
         * my best to avoid reversing a linked list.
         */
        current = -1;
        if (exit_cb != -1) {
          /* Note: I think i need some generic way to pass an event table to
           * callbacks regardless of the event type, then each callback can
           * extend it, for example, one field would be the `at` that represents
           * the clock. We'll see.
           */
          lua_rawgeti(klua_state, LUA_REGISTRYINDEX, exit_cb);
          lua_callk(klua_state, 0, 0, 0, NULL);
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
  vector_free(procs_vec);
  queue_free(interrupts_queue);
  return ret;
}
