#include <lua5.3/lua.h>
#include <lua5.3/lauxlib.h>
#include "process.h"
#include "vector.h"
#include <string.h>

extern Vector *procs_vec;
extern size_t running_process;
extern Process current;
extern int exit_cb;
extern int create_cb;
extern int init_cb;

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

/* Lua setCurrent function C implementation */
int klua_setCurrent(lua_State *klua_state) {
  klua_Process *proc_ud = luaL_checkudata(klua_state, 1, "Kernec.Process");
  Process p;
  vector_read(procs_vec, proc_ud->pptr, &p);
  running_process = 1;
  current = p;
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

