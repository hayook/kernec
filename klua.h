#ifndef KLUA_H
#define KLUA_H

#include <lua5.3/lua.h>

int klua_onEvent(lua_State *klua_state);
int klua_setCurrent(lua_State *klua_state);
int klua_pushprocess(lua_State *klua_state, int pptr);

#endif
