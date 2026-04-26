#include "kernec.h"
#include "process.h"
#include "vector.h"
#include <lua5.3/lauxlib.h>
#include <lua5.3/lua.h>
#include <stdlib.h>
#include <string.h>

extern Vector *procs_vec;
extern int current;
extern int exit_cb;
extern int create_cb;
extern int init_cb;

typedef struct {
  size_t pptr;
} klua_Process;

/* Lua onEvent function C implementation */
int klua_onEvent(lua_State *klua_state) {
  /* Lua onEvent arg1: event type (string) */
  const char *ev_type = luaL_checklstring(klua_state, 1, NULL);

  /* Lua onEvent arg2: callback (function) */
  luaL_checktype(klua_state, 2, LUA_TFUNCTION);

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
    /* To avoid a switch case for checking that the event is valid, then another
     * same one to store the cb, i have to create the reference before i check
     * whether the event is valid or no, so i need to unref it when the event is
     * invalid to avoid reference leaks. This may change when i refactor
     * callbacks management.
     */
    luaL_unref(klua_state, LUA_REGISTRYINDEX, cb);
    luaL_error(klua_state, "Unknown event \"%s\"", ev_type);
  }

  return 0;
}

/* Lua setCurrent function C implementation */
int klua_setCurrent(lua_State *klua_state) {
  klua_Process *proc_ud = luaL_checkudata(klua_state, 1, "Kernec.Process");
  current = proc_ud->pptr;
  return 0;
}

int klua_parseprogram(lua_State *klua_state, Program *pgm) {
  /* Stack: [ program_table ] */
  if (!lua_istable(klua_state, -1))
    return -1;

  /* Get program name */
  lua_getfield(klua_state, -1, "name");
  /* Stack: [ name program_table ] */
  if (!lua_isstring(klua_state, -1)) {
    lua_pop(klua_state, 1);
    return -1;
  }
  size_t name_len;
  const char *name = luaL_checklstring(klua_state, -1, &name_len);

  /* I am too lazy to allocate and manage memory for program names */
  if (name_len > NAME_BUF) {
    printf("ERROR: Program name is too long.\n");
    lua_pop(klua_state, 1);
    return -1;
  }

  strcpy(pgm->name, name);
  lua_pop(klua_state, 1);

  /* Parse program code */
  lua_getfield(klua_state, -1, "code");
  /* Stack: [ code_array program_table ] */
  if (!lua_istable(klua_state, -1)) {
    lua_pop(klua_state, 1);
    return -1;
  }
  size_t nb_blocks = lua_rawlen(klua_state, -1);
  pgm->code_size = nb_blocks;

  /* Note: I think i can ignore freeing this allocation, at least for now
   * as long as block arrays are fixed size and allocated only once, it
   * basically behaves like a static memory. Also note that they are shared with
   * Process.
   */
  pgm->code = (block *)malloc(nb_blocks * sizeof(block));
  for (int i = 0; i < nb_blocks; i++) {
    lua_geti(klua_state, -1, i + 1);
    /* Stack: [ block_i_table code_array program_table ] */
    if (!lua_istable(klua_state, -1)) {
      lua_pop(klua_state, 2);
      return -1;
    }

    /* Get op kind */
    lua_getfield(klua_state, -1, "op");
    /* Stack: [ op block_i_table code_array program_table ] */
    if (!lua_isstring(klua_state, -1)) {
      lua_pop(klua_state, 3);
      return -1;
    }
    const char *op = luaL_checklstring(klua_state, -1, NULL);
    if (strcmp(op, "CPU") == 0) {
      pgm->code[i].op = OP_CPU;
    } else {
      printf("ERROR: Parsing %s: Unknown block op '%s'\n", pgm->name, op);
      /* TODO: It's not convenient to count how many elements to pop each time i
       * early return when something wrong happens in the parsing.
       * Can i enforce a schema for the `job` global and let Lua VM checks the
       * type for me?
       */
      lua_pop(klua_state, 3);
      return -1;
    }

    lua_pop(klua_state, 1);

    /* Get op cost */
    lua_getfield(klua_state, -1, "cost");
    /* Stack: [ cost block_i_table code_array program_table ] */
    if (!lua_isnumber(klua_state, -1)) {
      lua_pop(klua_state, 3);
      return -1;
    }
    double cost = luaL_checknumber(klua_state, -1);
    pgm->code[i].cost = cost;
    lua_pop(klua_state, 1);

    lua_pop(klua_state, 1);
    /* Stack: [ code_array program_table ] */
  }
  lua_pop(klua_state, 1);

  /* Stack: [ program_table ] */
  return 0;
}

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
