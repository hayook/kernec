#include "kernec.h"
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

typedef struct {
  size_t pptr;
} klua_Process;

/* Lua onEvent function C implementation */
int klua_onEvent(lua_State *klua_state) {
  /* Lua onEvent arg1: event type (string) */
  if (!lua_isstring(klua_state, 1))
    return -1;

  size_t ev_type_len;
  const char *ev_type = lua_tolstring(klua_state, 1, &ev_type_len);

  /* Lua onEvent arg2: callback (function) */
  if (!lua_isfunction(klua_state, 2))
    return -1;

  /* TODO: Handle Lua errors, cuz the above checks are pretty much weak */

  int cb = luaL_ref(klua_state, LUA_REGISTRYINDEX);

  /* Save the callback */
  /* TODO: refactor how callbacks are managed */
  if (strcmp(ev_type, "exit") == 0) {
    exit_cb = cb;
  } else if (strcmp(ev_type, "create") == 0) {
    create_cb = cb;
  } else if (strcmp(ev_type, "init") == 0) {
    init_cb = cb;
  } else {
    return -1;
  }

  return 0;
}

Vector *procs_vec;

int klua_Process_index(lua_State *klua_state) {
  klua_Process *proc_ud = luaL_checkudata(klua_state, 1, "Kernec.Process");
  Process p;
  vector_read(procs_vec, proc_ud->pptr, &p);
  size_t len;
  const char *key = luaL_checklstring(klua_state, 2, &len);
  if (strcmp(key, "pid") == 0) {
    lua_pushinteger(klua_state, p.pid);
    return 1;
  } else {
    lua_getuservalue(klua_state, 1);
    lua_getfield(klua_state, -1, key);
    return 1;
  }
  return 0;
}

int klua_Process_newindex(lua_State *klua_state) {
  klua_Process *proc_ud = luaL_checkudata(klua_state, 1, "Kernec.Process");
  size_t len;
  const char *key = luaL_checklstring(klua_state, 2, &len);
  lua_getuservalue(klua_state, 1);
  lua_pushvalue(klua_state, 3);
  lua_setfield(klua_state, 4, key);
  return 0;
}

int klua_pushprocess(lua_State *klua_state, int pptr) {
  klua_Process *proc_ud = lua_newuserdata(klua_state, sizeof(klua_Process));
  proc_ud->pptr = pptr;

  /* Associate a uservalue */
  lua_newtable(klua_state);
  lua_setuservalue(klua_state, -2);

  /* klua_Process metatable */
  luaL_newmetatable(klua_state, "Kernec.Process");

  lua_pushcfunction(klua_state, klua_Process_index);
  lua_setfield(klua_state, -2, "__index");
  lua_pushcfunction(klua_state, klua_Process_newindex);
  lua_setfield(klua_state, -2, "__newindex");

  lua_setmetatable(klua_state, lua_gettop(klua_state) - 1);

  return 1;
}

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

int load_program(lua_State *klua_state, const char *name, const block code[],
                 int code_size) {
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

  if (vector_append(procs_vec, &p))
    return -1;

  int err = queue_append(ready_queue, &p);
  if (err)
    return -1;

  printf("[ OS ] Loading program '%s'...\n", name);
  printf("[ OS ] Process %lu created.\n", p.pid);
  printf("[ Scheduler ] Process %lu added to the ready queue\n", p.pid);

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

  procs_vec = vector_new(sizeof(Process));
  ready_queue = queue_new(sizeof(Process));

  interrupts_queue = queue_new(sizeof(Event));
  if (!interrupts_queue) {
    printf("ERROR: failed to create the events queueu\n");
    goto out_free;
  }

  /* Lua init */
  lua_State *klua_state = luaL_newstate();
  luaL_openlibs(klua_state);
  lua_register(klua_state, "onEvent", klua_onEvent);
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
  err = load_program(klua_state, "Web Browser", p1_code,
                     sizeof(p1_code) / sizeof(p1_code[0]));
  if (err) {
    printf("Can't load program\n");
    goto out_free;
  }

  err = load_program(klua_state, "Web Browser", p1_code,
                     sizeof(p1_code) / sizeof(p1_code[0]));
  if (err) {
    printf("Can't load program\n");
    goto out_free;
    return 1;
  }

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
        if (exit_cb != -1) {
          /* Note: I think i need some generic way to pass an event table to
           * callbacks regardless of the event type, then each callback can
           * extend it, for example, one field would be the `at` that represents
           * the clock. We'll see.
           */
          lua_rawgeti(klua_state, LUA_REGISTRYINDEX, exit_cb);
          lua_callk(klua_state, 0, 0, 0, NULL);
        }
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
  vector_free(procs_vec);
  queue_free(ready_queue);
  queue_free(interrupts_queue);
  return ret;
}
