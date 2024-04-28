#include "luastate.h"

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
};

#include "luahelpers.h"

/* ------------------------------------------------------------------------------------------------ LuaState::init
 */
b8 LuaState::init(Logger::Verbosity verbosity)
{
	logger.init("luastate"_str, verbosity);

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
s32 LuaState::gettop()
{
	return lua_gettop(L);
}

/* ------------------------------------------------------------------------------------------------ LuaState::tostring
 */
str LuaState::tostring(s32 idx)
{
	size_t len;
	const char* s = lua_tolstring(L, idx, &len);
	return {(u8*)s, (s64)len};
}

/* ------------------------------------------------------------------------------------------------ LuaState::newtable
 */
void LuaState::newtable()
{
	lua_newtable(L);
}

/* ------------------------------------------------------------------------------------------------ LuaState::settable
 */
void LuaState::settable(s32 idx)
{
	lua_settable(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::gettable
 */
void LuaState::gettable(s32 idx)
{
	lua_gettable(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::setglobal
 */
void LuaState::setglobal(const char* name)
{
	lua_setglobal(L, name);
}

/* ------------------------------------------------------------------------------------------------ LuaState::getglobal
 */
void LuaState::getglobal(const char* name)
{
	lua_getglobal(L, name);
}

/* ------------------------------------------------------------------------------------------------ LuaState::objlen
 */
s32 LuaState::objlen(s32 idx)
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
		ERROR("load '", name, "' failed:\n", tostring(), "\n");
		pop();
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::loadbuffer
 */
b8 LuaState::loadbuffer(str buffer, const char* name)
{
	auto loader = [](lua_State* L, void* data, size_t* size) -> const char* 
	{
		str* buffer = (str*)data;
		*size = buffer->len;
		return (char*)buffer->bytes;
	};

	if (lua_load(L, loader, &buffer, name))
	{
		ERROR("loadbuffer '", name, "' failed:\n", tostring(), "\n"); 
		pop();
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::loadfile
 */
b8 LuaState::loadfile(const char* path)
{
	if (luaL_loadfile(L, path))
	{
		ERROR("loadfile '", path, "' failed: \n", tostring(), "\n");
		pop();
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::loadstring
 */
b8 LuaState::loadstring(const char* s)
{
	if (luaL_loadstring(L, s))
	{
		ERROR("failed to load string:\n", tostring(), "\n");
		pop();
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::dofile
 */
b8 LuaState::dofile(const char* s)
{
	if(luaL_dofile(L, s))
	{
		ERROR("dofile(", s, ") failed:\n", tostring(), "\n");
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
		ERROR(tostring(), "\n");
		pop();
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::getfenv
 */
void LuaState::getfenv(s32 idx)
{
	lua_getfenv(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::setfenv
 */
b8 LuaState::setfenv(s32 idx)
{
	if (!lua_setfenv(L, idx))
	{
		ERROR("setfenv failed:\n", tostring(), "\n");
		pop();
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------------------------------------ LuaState::pushstring
 */
void LuaState::pushstring(str s)
{
	// TODO(sushi) handle non-temrinated input
	lua_pushlstring(L, (char*)s.bytes, s.len);
}

/* ------------------------------------------------------------------------------------------------ LuaState::pushlightuserdata
 */
void LuaState::pushlightuserdata(void* data)
{
	lua_pushlightuserdata(L, data);
}

/* ------------------------------------------------------------------------------------------------ LuaState::pushinteger
 */
void LuaState::pushinteger(s32 i)
{
	lua_pushinteger(L, i);
}

/* ------------------------------------------------------------------------------------------------ LuaState::pushvalue
 */
void LuaState::pushvalue(s32 idx)
{
	lua_pushvalue(L, idx);
}

/* ------------------------------------------------------------------------------------------------ LuaState::isnil
 */
b8 LuaState::isnil(s32 idx)
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

		if (!dest->write({(u8*)p, (s64)sz}))
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

/* ------------------------------------------------------------------------------------------------ LuaState::stack_dump
 */
void LuaState::stack_dump(u32 max_depth)
{
	::stack_dump(L, max_depth);
}
