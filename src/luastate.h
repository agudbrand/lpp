/*
 *  Wrapper around the lua state to help reduce a lot of duplicated code,
 *  primarily all the error checking I have to do in most calls.
 *
 */

#ifndef _lpp_luastate_h
#define _lpp_luastate_h

#include "common.h"
#include "logger.h"

using namespace iro;

struct lua_State;

/* ================================================================================================ LuaState
 */
struct LuaState
{
	lua_State* L;

	Logger logger;
	
	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */

	b8   init(Logger::Verbosity verbosity = Logger::Verbosity::Warn);
	void deinit();

	void pop(s32 count = 1);

	s32 gettop();

	str tostring(s32 idx = -1);

	void newtable();

	// Does t[k] = v where v is the value at the top of the stack, k is the 
	// value just below the top, and t is the table at the given index.
	// Pops the key and value from the stack.
	void settable(s32 idx);

	// Does t[k] where k is the value at the top of the stack and t
	// is table at the given index. Leaves the result on the top 
	// of the stack and pops k.
	void gettable(s32 idx);

	void setglobal(const char* name);
	void getglobal(const char* name);

	s32 objlen(s32 idx = -1);

	b8 load(io::IO* in, const char* name);
	b8 loadbuffer(str buffer, const char* name);
	b8 loadfile(const char* path);
	b8 loadstring(const char* s);
	b8 dofile(const char* s);

	b8 pcall(s32 nargs = 0, s32 nresults = 0, s32 errfunc = 0);

	// pushes onto the stack the environment table of the value 
	// at the given index
	void getfenv(s32 idx);
	// pops table at the top of the stack and sets it as the environment 
	// for the value at the given index
	b8 setfenv(s32 idx);

	void pushstring(str s);
	void pushlightuserdata(void* data);
	void pushinteger(s32 i);
	void pushvalue(s32 idx);

	b8 isnil(s32 idx = -1);

	b8 dump(io::IO* dest);

	void stack_dump(u32 max_depth = -1);
};

#endif // _lpp_luastate_h
