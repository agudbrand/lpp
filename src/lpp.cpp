#include "lpp.h"

#include "logger.h"

#include "stdio.h"

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
};

#include "assert.h"

/* ------------------------------------------------------------------------------------------------ Lpp::init
 */
b8 Lpp::init(Logger::Verbosity verbosity)
{
    logger.init("lpp"_str, verbosity);

	INFO("init");

	DEBUG("creating lua state\n");
	L = lua_open();
	luaL_openlibs(L);

	if (!cache_metaenvironment())
		return false;

    return true;
}

/* ------------------------------------------------------------------------------------------------ Lpp::init
 */
void Lpp::deinit()
{
	lua_close(L);
	metaenv_chunk.close();
}

/* ------------------------------------------------------------------------------------------------ Lpp::create_metaprogram
 */
b8 Lpp::create_metaprogram(
		str     name,
		io::IO* input_stream, 
		io::IO* output_stream)
{
	INFO("creating metaprogram from input stream '", name, "'\n");

	Parser parser;
	if (!parser.init(input_stream, name, output_stream, logger.verbosity))
		return false;

	if (setjmp(parser.err_handler))
	{
		parser.deinit();
		return false;
	}

	if (!parser.run())
		return false;

	return true;
}

/* ------------------------------------------------------------------------------------------------ metaprogram_reader
 */
struct ReaderData
{
	u8* buffer;
	io::IO* io;
};

static const s64 reader_buffer_size = 256;

const char* metaprogram_reader(lua_State* L, void* data, size_t* size)
{
	ReaderData* in = (ReaderData*)data;
	*size = in->io->read({in->buffer, reader_buffer_size});
	return (const char*)in->buffer;
}

/* ------------------------------------------------------------------------------------------------ Lpp::run_metaprogram
 */
b8 Lpp::run_metaprogram(
		str     name,
		io::IO* input_stream, 
		io::IO* output_stream)
{
	INFO("running metaprogram '", name, "'\n");

	u8 reader_buffer[reader_buffer_size];
	ReaderData data = {reader_buffer, input_stream};

	if (lua_load(L, metaprogram_reader, &data, (char*)name.bytes)) // TODO(sushi) idk handle non null-terminated names later if its ever needed
	{
		ERROR("failed to load metaprogram '", name, "':\n", lua_tostring(L, -1), "\n");
		lua_pop(L, 1);
		return false;
	}

	if (!load_metaenvironment())
		return false;

	if (lua_pcall(L, 0, 2, 0))
	{
		ERROR("failed to run metaenvironment:\n", lua_tostring(L, -1), "\n");
		lua_pop(L, 1);
		return false;
	}

	// the lpp table is at the top of the stack and we need to give it 
	// a handle to this context
	lua_pushstring(L, "handle");
	lua_pushlightuserdata(L, this);
	lua_settable(L, -3);
	lua_pop(L, 1);

	if (!lua_setfenv(L, 1))
	{
		ERROR("failed to set environment of metaprogram '", name, "':\n", lua_tostring(L, -1), "\n");
		lua_pop(L, 1);
		return false;
	}

	if (lua_pcall(L, 0, 1, 0))
	{
		ERROR("failed to run metaprogram:\n", lua_tostring(L, -1), "\n");
		lua_pop(L, 1);
		return false;
	}

	size_t len;
	const char* s = lua_tolstring(L, -1, &len);

	output_stream->write({(u8*)s, (s64)len});

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lpp::cache_writer
 */
int Lpp::cache_writer(lua_State* L, const void* p, size_t sz, void* ud)
{
	Lpp* lpp = (Lpp*)ud;

	if (!lpp->metaenv_chunk.write({(u8*)p, (s64)sz}))
		return 1;

	return 0;
}

/* ------------------------------------------------------------------------------------------------ Lpp::cache_metaenvironment
 */
b8 Lpp::cache_metaenvironment()
{
	DEBUG("caching metaenvironment\n");

	if (luaL_loadfile(L, "src/metaenv.lua"))
	{
		ERROR("failed to load metaenvironment:\n", lua_tostring(L, -1), "\n");
		lua_pop(L, 1);
		return false;
	}

	
	DEBUG("initializing metaenv chunk\n");
	metaenv_chunk.open();

	DEBUG("dumping metaenvironment\n");
	if (lua_dump(L, cache_writer, this))
	{
		ERROR("failed to dump metaenvironment\n");
		return false;
	}

	lua_pop(L, 1);

	return true;
}

/* ------------------------------------------------------------------------------------------------ metaenvironment_loader
 */
const char* metaenvironment_loader(lua_State* L, void* data, size_t* size)
{
	io::Memory* m = (io::Memory*)data;
	*size = m->len;
	return (const char*)m->buffer;
}

/* ------------------------------------------------------------------------------------------------ Lpp::load_metaenvironment
 */
b8 Lpp::load_metaenvironment()
{
	if (lua_load(L, metaenvironment_loader, &metaenv_chunk, "metaenvironment"))
	{
		ERROR("failed to load cached metaenvironment:\n", lua_tostring(L, -1), "\n");
		lua_pop(L, 1);
		return false;
	}
}

/* ================================================================================================ lua C API
 *  C api exposed to the internal lpp module 
 */
extern "C"
{

str get_token_indentation(Lpp* lpp, s32 idx)
{
	return ""_str; //lpp->tokens[idx].indentation;
}

struct ParseContext
{
	
};

void* parse_file(Lpp* lpp, str name)
{
	io::FileDescriptor f;
	if (!f.open(name, io::Flag::Readable))
		return str::invalid();

	io::Memory mp;
	mp.open();

	if (!lpp->create_metaprogram(name, &f, &m))
		return str::invalid();

	io::Memory fin;
	fin.open(mp.len);


}

}


