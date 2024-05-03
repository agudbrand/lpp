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

	s32 getTop();

	str toString(s32 idx = -1);

	void newTable();

	// Does t[k] = v where v is the value at the top of the stack, k is the 
	// value just below the top, and t is the table at the given index.
	// Pops the key and value from the stack.
	void setTable(s32 idx);

	// Does t[k] where k is the value at the top of the stack and t
	// is table at the given index. Leaves the result on the top 
	// of the stack and pops k.
	void getTable(s32 idx);

	void setGlobal(const char* name);
	void getGlobal(const char* name);

	s32 objLen(s32 idx = -1);

	b8 load(io::IO* in, const char* name);
	b8 loadBuffer(str buffer, const char* name);
	b8 loadFile(const char* path);
	b8 loadString(const char* s);
	b8 doFile(const char* s);

	b8 pcall(s32 nargs = 0, s32 nresults = 0, s32 errfunc = 0);
	b8 callMeta(const char* name, s32 idx = -1);

	// pushes onto the stack the environment table of the value 
	// at the given index
	void getfEnv(s32 idx);
	// pops table at the top of the stack and sets it as the environment 
	// for the value at the given index
	b8 setfEnv(s32 idx);

	void pushString(str s);
	void pushLightUserdata(void* data);
	void pushInteger(s32 i);
	void pushValue(s32 idx);

	b8 isNil(s32 idx = -1);

	b8 dump(io::IO* dest);

	void stackDump(u32 max_depth = -1);
};

#endif // _lpp_luastate_h
