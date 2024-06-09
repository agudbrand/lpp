#include "lpp.h"

#include "iro/logger.h"
#include "iro/fs/fs.h"

#include "assert.h"

#include "metaenvironment.h"

extern "C"
{
#include "lua.h"
}

const char* lpp_metaenv_stack = "__lpp_metaenv_stack";

/* ------------------------------------------------------------------------------------------------ Lpp::init
 */
b8 Lpp::init(Logger::Verbosity verbosity)
{
    logger.init("lpp"_str, verbosity);

	INFO("init\n");

	DEBUG("creating lua state\n");
	lua.init();

	DEBUG("creating metaenv stack\n");
	lua.newTable();
	lua.setGlobal(lpp_metaenv_stack);

	DEBUG("creating pools\n");
	context_pool = Pool<MetaprogramContext>::create();
	contexts = DList<MetaprogramContext>::create();
	source_pool = Pool<Source>::create();
	sources = SList<Source>::create();
	metaenv_pool = Pool<Metaenvironment>::create();
	metaenvs = SList<Metaenvironment>::create();

	DEBUG("loading luajit ffi\n");
	if (!lua.doFile("src/cdefs.lua"))
	{
		ERROR("failed to load luajit ffi\n");
		return false;
	}

	if (!cacheMetaenvironment())
		return false;

    return true;
}

/* ------------------------------------------------------------------------------------------------ Lpp::init
 */
void Lpp::deinit()
{
	lua.deinit();
	contexts.destroy();
	context_pool.destroy();
	for (auto& source : sources)
		source.deinit();
	sources.destroy();
	source_pool.destroy();
	for (auto& metaenv : metaenvs)
		metaenv.deinit();
	metaenvs.destroy();
	metaenv_pool.destroy();

	metaenv_chunk.close();
	*this = {};
}

/* ------------------------------------------------------------------------------------------------ Lpp::createMetaprogram
 */
Metaprogram Lpp::createMetaprogram(
		str     name,
		io::IO* instream, 
		io::IO* outstream)
{
	DEBUG("creating metaprogram from input stream '", name, "'\n");

	Source* source = source_pool.add();
	sources.push(source);
	source->init(name);

	Parser parser;
	if (!parser.init(source, instream, outstream, logger.verbosity))
		return nullptr;
	defer { parser.deinit(); };

	if (setjmp(parser.err_handler))
		return nullptr;

	if (!parser.run())
		return nullptr;

	Source* dest = source_pool.add();
	sources.push(dest);
	dest->init("dest"_str);

	Metaenvironment* metaenv = metaenv_pool.add();
	metaenvs.push(metaenv);
	metaenv->init(this, source, dest);

	MetaprogramContext* ctx = context_pool.add();
	contexts.pushHead(ctx);
	ctx->lpp = this;
	ctx->metaenv = metaenv;

	return ctx;
}

/* ------------------------------------------------------------------------------------------------ Lpp::runMetaprogram
 */
b8 Lpp::runMetaprogram(
		Metaprogram metaprogram,
		io::IO* input_stream, 
		io::IO* output_stream)
{
	assert(metaprogram && input_stream && output_stream);

	MetaprogramContext* ctx = (MetaprogramContext*)metaprogram;

	INFO("running metaprogram '", ctx->metaenv->input->name, "'\n");

	if (!lua.load(input_stream, (const char*)ctx->metaenv->input->name.bytes))
	{
		ERROR("failed to load metaprogram\n");
		return false;
	}

	if (!loadMetaenvironment())
		return false;

	if (!lua.pcall(0, 2))
	{
		ERROR("failed to run metaenvironment\n");
		return false;
	}

	// the lpp table is at the top of the stack and we need to give it 
	// a handle to this context
	lua.pushString("context"_str);
	lua.pushLightUserdata(ctx);
	lua.setTable(-3);
	lua.pop();

	// set the new metaenvironment's __index to point 
	// at the previous one so that any global variables 
	// declared in it will be available
	lua.getGlobal(lpp_metaenv_stack);
	s32 stacklen = lua.objLen();
	lua.pushString("__index"_str);
	lua.pushInteger(stacklen);
	lua.getTable(lua.getTop()-2);

	if (!lua.isNil())
		lua.setTable(-4);
	else
		lua.pop(2);

	lua.pushInteger(stacklen+1);
	lua.pushValue(-3);
	lua.setTable(-3);
	lua.pop();

	if (!lua.setfEnv(-2))
	{
		ERROR("failed to set environment of metaprogram\n");
		return false;
	}

	if (!lua.pcall())
	{
		ERROR("failed to run metaprogram\n");
		return false;
	}

	lua.getGlobal(lpp_metaenv_stack);
	lua.pushInteger(lua.objLen());
	lua.getTable(-2);
	lua.pushString("__metaenv"_str);
	lua.getTable(-2);

	if (!ctx->metaenv->processSections())
		return false;

	lua.pop(2);

	output_stream->write(ctx->metaenv->output->cache.asStr());
	lua.pop();

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lpp::cacheWriter
 */
int Lpp::cacheWriter(lua_State* L, const void* p, size_t sz, void* ud)
{
	Lpp* lpp = (Lpp*)ud;

	if (!lpp->metaenv_chunk.write({(u8*)p, sz}))
		return 1;

	return 0;
}

/* ------------------------------------------------------------------------------------------------ Lpp::cacheMetaenvironment
 */
b8 Lpp::cacheMetaenvironment()
{
	DEBUG("caching metaenvironment\n");

	if (!lua.loadFile("src/metaenv.lua"))
	{
		ERROR("failed to load metaenvironment\n");
		return false;
	}
	
	DEBUG("initializing metaenv chunk\n");
	metaenv_chunk.open();

	DEBUG("dumping metaenvironment\n");
	if (!lua.dump(&metaenv_chunk))
	{
		ERROR("failed to dump metaenvironment\n");
		return false;
	}

	lua.pop();

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lpp::load_metaenvironment
 */
b8 Lpp::loadMetaenvironment()
{
	defer { metaenv_chunk.rewind(); };

	if (!lua.load(&metaenv_chunk, "metaenvironment"))
	{
		ERROR("failed to load cached metaenvironment\n");
		return false;
	}

	return true;
}

/* ================================================================================================ lua C API
 *  C api exposed to the internal lpp module 
 */
extern "C"
{

struct MetaprogramBuffer
{
	io::Memory* memhandle;
	u64         memsize;
};

/* ------------------------------------------------------------------------------------------------ processFile
 *  Wrapper around an lpp context's create_metaprogram() and run_metaprogram(). This creates a 
 *  metaprogram, executes it, and then returns a handle to the memory buffer its stored in along 
 *  with the size needed to store it in memory. This must be passed back into 
 *  get_metaprogram_result() along with a luajit string buffer with enough space to fit the 
 *  program.
 *
 *  TODO(sushi) this seems really inefficient, but I think it should work fine as a first pass
 *              and can be made more efficient/elegant later.
 *              We could maybe create an io::IO that wraps a luajit string buffer and feed 
 *              this information directly to it, but that's for another time.
 *  TODO(sushi) we can maybe add a bool to run_metaprogram to instruct it to leave the metaprogram 
 *              on the lua stack instead of popping it when its finished. I don't know if doing 
 *              this is fine during a luajit ffi call, though, and I can't seem to find anyone 
 *              on the internet or in the luajit docs mentioning doing something similar.
 *              -- OK NOW this is not as possible because i've changed how buffers work,
 *                 buffer data is no longer stored in lua, we store it in Metaenvironment::Sections.
 *                 So this might just be how this has to be for now until I think of a better solution.
 *                 This might not even be that much better than making a lua string and passing it 
 *                 directly!!!!
 *  TODO(sushi) this is so dumb, just make this a normal lua C function and pass the result back
 *              as a string.
 */ 
MetaprogramBuffer processFile(MetaprogramContext* ctx, str path)
{
	Lpp* lpp = ctx->lpp;

	auto f = scoped(fs::File::from(path, fs::OpenFlag::Read));
	if (isnil(f))
		return {};

	io::Memory mp;
	mp.open();
	defer { mp.close(); };

	Metaprogram m = lpp->createMetaprogram(path, &f, &mp);
	if (!m)
		return {};
	defer 
	{ 
		lpp->contexts.remove(ctx->list_node);
		lpp->context_pool.remove((MetaprogramContext*)m); 
	};

	// not a fan of making 2 mem buffers here
	// (actually, 3, because the lexer uses one internally to keep token raws around
	//  but ideally we clean that up later on to not have to store so much)
	// also i dont like using 'new' here, but i dont want to make something 
	// to keep track of this stuff internally atm so maybe ill do that later, yeah
	// that sounds like a good idea.
	io::Memory* result = new io::Memory;
	result->open();

	if (!lpp->runMetaprogram(m, &mp, result))
		return {};

	MetaprogramBuffer out;
	out.memhandle = result;
	out.memsize = result->len;
	return out;
}

/* ------------------------------------------------------------------------------------------------ getMetaprogramResult
 *  Copies into 'outbuf' the metaprogram stored in 'mpbuf'. If the metaprogram cannot be retrieved 
 *  for some reason then 'outbuf' should be null. Frees the metaprogram memory in anycase.
 */
void getMetaprogramResult(MetaprogramBuffer mpbuf, void* outbuf)
{
	if (outbuf)
	{
		mpbuf.memhandle->read({(u8*)outbuf, mpbuf.memsize});
	}

	mpbuf.memhandle->close();
}

}


