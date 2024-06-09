#include "luastate.h"

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
};

#include "luahelpers.h"

static Logger logger = Logger::create("luastate"_str, Logger::Verbosity::Info);

/* ------------------------------------------------------------------------------------------------ LuaState::init
 */
b8 LuaState::init()
{
	L = lua_open();
	luaL_openlibs(L);

	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::deinit
 */
void LuaState::deinit()
{
	lua_close(L);
	L = nullptr;
}

/* ------------------------------------------------------------------------------------------------ LuaState::pop
 */
void LuaState::pop(s32 count)
{
	lua_pop(L, count);
}

/* ------------------------------------------------------------------------------------------------ LuaState::gettop
 */
s32 LuaState::getTop()
{
	return lua_gettop(L);
}

/* ------------------------------------------------------------------------------------------------ LuaState::tostring
 */
str LuaState::toString(s32 idx)
{
	size_t len;
	const char* s = lua_tolstring(L, idx, &len);
	return {(u8*)s, len};
}

/* ------------------------------------------------------------------------------------------------ LuaState::newtable
 */
void LuaState::newTable()
{
	lua_newtable(L);
}

/* ------------------------------------------------------------------------------------------------ LuaState::settable
 */
void LuaState::setTable(s32 idx)
{
	lua_settable(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::gettable
 */
void LuaState::getTable(s32 idx)
{
	lua_gettable(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::setglobal
 */
void LuaState::setGlobal(const char* name)
{
	lua_setglobal(L, name);
}

/* ------------------------------------------------------------------------------------------------ LuaState::getglobal
 */
void LuaState::getGlobal(const char* name)
{
	lua_getglobal(L, name);
}

/* ------------------------------------------------------------------------------------------------ LuaState::objlen
 */
s32 LuaState::objLen(s32 idx)
{
	return lua_objlen(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::load
 */
b8 LuaState::load(io::IO* in, const char* name)
{
	static const s64 reader_buffer_size = 128;
	u8 reader_buffer[reader_buffer_size];
	struct D { u8* buffer; io::IO* io; } d {reader_buffer, in};

	auto reader = [](lua_State* L, void* data, size_t* size) -> const char*
	{
		D* d = (D*)data;
		*size = d->io->read({d->buffer, reader_buffer_size});
		return (const char*)d->buffer;
	};

	if (lua_load(L, reader, &d, name))
	{
		ERROR("load '", name, "' failed:\n", toString(), "\n");
		pop();
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::loadbuffer
 */
b8 LuaState::loadBuffer(str buffer, const char* name)
{
	auto loader = [](lua_State* L, void* data, size_t* size) -> const char* 
	{
		str* buffer = (str*)data;
		*size = buffer->len;
		return (char*)buffer->bytes;
	};

	if (lua_load(L, loader, &buffer, name))
	{
		ERROR("loadbuffer '", name, "' failed:\n", toString(), "\n"); 
		pop();
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::loadfile
 */
b8 LuaState::loadFile(const char* path)
{
	if (luaL_loadfile(L, path))
	{
		ERROR("loadfile '", path, "' failed: \n", toString(), "\n");
		pop();
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::loadstring
 */
b8 LuaState::loadString(const char* s)
{
	if (luaL_loadstring(L, s))
	{
		ERROR("failed to load string:\n", toString(), "\n");
		pop();
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::dofile
 */
b8 LuaState::doFile(const char* s)
{
	if(luaL_dofile(L, s))
	{
		ERROR("dofile(", s, ") failed:\n", toString(), "\n");
		pop();
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::pcall
 */
b8 LuaState::pcall(s32 nargs, s32 nresults, s32 errfunc)
{
	if (lua_pcall(L, nargs, nresults, errfunc))
	{
		ERROR(toString(), "\n");
		pop();
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::callmeta
 */
b8 LuaState::callMeta(const char* name, s32 idx)
{
	if (!luaL_callmeta(L, idx, name))
		return false;
	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::getfenv
 */
void LuaState::getfEnv(s32 idx)
{
	lua_getfenv(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::setfenv
 */
b8 LuaState::setfEnv(s32 idx)
{
	if (!lua_setfenv(L, idx))
	{
		ERROR("setfenv failed:\n", toString(), "\n");
		pop();
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::pushstring
 */
void LuaState::pushString(str s)
{
	// TODO(sushi) handle non-temrinated input
	lua_pushlstring(L, (char*)s.bytes, s.len);
}

/* ------------------------------------------------------------------------------------------------ LuaState::pushlightuserdata
 */
void LuaState::pushLightUserdata(void* data)
{
	lua_pushlightuserdata(L, data);
}

/* ------------------------------------------------------------------------------------------------ LuaState::pushinteger
 */
void LuaState::pushInteger(s32 i)
{
	lua_pushinteger(L, i);
}

/* ------------------------------------------------------------------------------------------------ LuaState::pushvalue
 */
void LuaState::pushValue(s32 idx)
{
	lua_pushvalue(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::isnil
 */
b8 LuaState::isNil(s32 idx)
{
	return lua_isnil(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::dump
 */
b8 LuaState::dump(io::IO* dest)
{
	auto writer = [](lua_State* L, const void* p, size_t sz, void* ud) ->int
	{
		io::IO* dest = (io::IO*)ud;

		if (!dest->write({(u8*)p, sz}))
			return 1;
		return 0;
	};

	if (lua_dump(L, writer, dest))
	{
		ERROR("dump failed\n");
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::stackDump
 */
void LuaState::stackDump(u32 max_depth)
{
	::stack_dump(L, max_depth);
}

/* ------------------------------------------------------------------------------------------------ LuaState::installDebugHook
 */
void LuaState::installDebugHook()
{
	auto hook = [](lua_State* L, lua_Debug* ar) -> void
	{
		lua_getinfo(L, "nSlu", ar);

		b8 leaving_func = ar->event == LUA_HOOKRET;

		if (ar->event == LUA_HOOKCALL)
			INFO("calling ");
		else
			INFO("leaving ");

		switch (cStrHash(ar->what))
		{
		case "main"_hashed:
			INFO("main chunk\n");
			break;

		case "Lua"_hashed:
			INFO("lua function\n");
			break;

		case "C"_hashed:
			INFO("C function\n");
			break;

		case "tail"_hashed:
			INFO("tail call\n");
			break;
		}

		if (ar->name)
		{
			INFO("function name: ", ar->name, "\n");
		}
	};

	if (lua_sethook(L, hook, LUA_MASKCALL | LUA_MASKRET, 0))
	{
		ERROR("failed to install debug hook!\n");
	}
}
