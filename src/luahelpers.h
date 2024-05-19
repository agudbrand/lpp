#ifndef _lpp_luahelpers_h
#define _lpp_luahelpers_h

#include "iro/common.h"

extern "C"
{

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

static void
print_lua_value(lua_State* L, int idx) {
	int t = lua_type(L, idx);
	switch(t) {
		case LUA_TTABLE: {
			printf("lua_to_str8 does not handle tables, `print_table` should have been called for this element.");
		} break;
		case LUA_TNIL: printf("nil"); return;
		case LUA_TNUMBER: printf("%f", lua_tonumber(L, idx)); return;
		case LUA_TSTRING: printf("\"%s\"", lua_tostring(L, idx)); return;
		case LUA_TBOOLEAN: printf(lua_toboolean(L, idx) ? "true" : "false"); return;
		case LUA_TTHREAD: printf("<lua thread>"); return;
		case LUA_TFUNCTION: printf("<lua function>"); return;
		default: {
			printf("unhandled lua type in lua_to_str8: %s", lua_typename(L, t));
		} break;
	}
}

static void
print_lua_table_interior(lua_State* L, int idx, u32 layers, u32 depth) {
#define do_indent \
	for(u32 i = 0; i < layers; i++) printf(" ");
	
	lua_pushvalue(L, idx);
	lua_pushnil(L);
	while(lua_next(L, -2)) {
		int kt = lua_type(L, -2);
		int vt = lua_type(L, -1);
		do_indent;
		print_lua_value(L, -2);
		printf(" = ");
		if(vt == LUA_TTABLE) {
			if (layers >= depth)
			{
				printf("...\n");
			}
			else
			{
				printf("{\n");
				print_lua_table_interior(L, -1, layers + 1, depth);
				do_indent;
				printf("}\n");
			}
		} else {
			print_lua_value(L, -1);
			printf("\n");
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

#undef do_indent
}

static void
print_lua_table(lua_State* L, int idx, u32 depth) {
	printf("{\n");
	print_lua_table_interior(L, idx, 1, depth);
	printf("}\n");
}

static void
stack_dump(lua_State* L, u32 max_depth) {
	printf("-------------\n");
	int top = lua_gettop(L);
	for(int i = 1; i <= top; i++) {
		int t = lua_type(L, i);
		printf("i %i: ", i);
		if(t==LUA_TTABLE) {
			print_lua_table(L, i, max_depth);
		} else {
			print_lua_value(L, i);
		}
		printf("\n");
	}
}

}

#endif
