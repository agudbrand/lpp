#include "lpp.h"

#include "iro/logger.h"
#include "iro/fs/fs.h"

#include "assert.h"

#include "metaenvironment.h"

extern "C"
{
#include "lua.h"
}

static Logger logger = Logger::create("lpp"_str, Logger::Verbosity::Debug);

const char* lpp_metaenv_stack = "__lpp_metaenv_stack";

/* ------------------------------------------------------------------------------------------------ Lpp::init
 */
b8 Lpp::init()
{
	INFO("init\n");

	DEBUG("creating lua state\n");
	lua.init();

	DEBUG("creating metaenv stack\n");
	lua.newtable();
	lua.setglobal(lpp_metaenv_stack);

	DEBUG("creating pools\n");
	context_pool = Pool<MetaprogramContext>::create();
	contexts = DList<MetaprogramContext>::create();
	source_pool = Pool<Source>::create();
	sources = SList<Source>::create();
	metaenv_pool = Pool<Metaenvironment>::create();
	metaenvs = SList<Metaenvironment>::create();

	DEBUG("loading luajit ffi\n");
	if (!lua.dofile("src/cdefs.lua"))
	{
		ERROR("failed to load luajit ffi\n");
		return false;
	}

	if (!lua.require("lpp"_str))
		return false;

	// give lpp module a handle to us
	lua.pushstring("handle"_str);
	lua.pushlightuserdata(this);
	lua.settable(lua.gettop()-2);
	lua.pop();

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
	if (!parser.init(source, instream, outstream))
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
	ctx->list_node = contexts.pushHead(ctx);
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

	const s32 luabottom = lua.gettop();
	defer { lua.settop(luabottom); };

	// TODO(sushi) maybe just make an actual Metaprogram type and 
	//             put this logic there.

	// Metaprogram phase 1
	//
	// Evaluate lua code and construct section list.
	{
		if (!lua.load(input_stream, (const char*)ctx->metaenv->input->name.bytes))
		{
			ERROR("failed to load metaprogram\n");
			return false;
		}
		const s32 metaprogram_idx = lua.gettop();

		// Get the metaenv construction function and call it to 
		// make a new environment.
		if (!lua.require("metaenv"_str))
		{
			ERROR("failed to load metaenvironment module\n");
			return false;
		}

		// Call construction function with context.
		
		lua.pushlightuserdata(ctx);
		if (!lua.pcall(1, 1))
		{
			ERROR("metaenvironment construction function failed\n");
			return false;
		}
		const s32 metaenv_table_idx = lua.gettop();

		// Set the new metaenvironment's __index to point 
		// at the previous one so that any global variables 
		// declared in it will be available.
		
		lua.getglobal(lpp_metaenv_stack);
		const s32 metaenv_stack_idx = lua.gettop();
		s32 me_stacklen = lua.objlen(metaenv_stack_idx);
		
		const s32 __index_string_idx = lua.gettop();

		lua.pushinteger(me_stacklen);

		lua.gettable(metaenv_stack_idx);
		const s32 last_metaenv_idx = lua.gettop();

		if (!lua.isnil())
		{
#if 0
			// TODO(sushi) I'm stupid and this doesn't work.
			//             Because a macro is evaluated after the lua code has been executed, whatever 
			//             the last value of a global is is what a file passed to processFile will 
			//             see, this allows stuff like: 
			//
			//               ---- import.lpp ----
			//			       
			//			       $ DEBUG = false
			//
			//			       @import "test.lpp"
			//                 
			//                 $ DEBUG = true
			//			       
			//			       int main() {}
			//
			//               ---- test.lpp ----
			//
			//                 void printMode()
			//                 {
			//                   $ if DEBUG then
			//                     printf("debug\n");
			//                   $ else
			//                     printf("release\n");  
			//                   $ end
			//                 }
			//
			//               ---- result ----
			//   
			//                 void printMode()
			//                 {
			//                   printf("debug\n");
			//                 }
			//
			//                 int main() {}
			//           
			//           I see this as incorrect behavior, and so it needs to be fixed somehow. The only
			//           way I can think to handle it properly, since this doesnt work, is to create
			//           snapshots of the metaenv's globals at each macro invocation. That kinda really
			//           sucks, though. I managed to compress how large the environment table is to
			//           only require two non-user elements, so its not like it would be a HUGE deal..
			//           maybe. 
			//
			//
			// Make a shallow copy of the current metaenv so that 
			// we can avoid changing a global in an importer's file
			// affecting the result of a file it imports.
			lua.newtable();
			const s32 copy_idx = lua.gettop();
			
			lua.pairs(last_metaenv_idx, [&]()
				{
					lua.pushvalue(-2);
					lua.pushvalue(-2);
					lua.settable(copy_idx);
					return true;
				});
#else
			lua.pushstring("__index"_str);
			lua.insert(-2); // swap key and value
#endif

			lua.stackDump();

			// set its index
			lua.settable(metaenv_table_idx);

		}
		else
			// remove unused values
			lua.pop();
		
		// push the metaenv onto the stack
		
		lua.pushinteger(me_stacklen + 1);
		lua.pushvalue(metaenv_table_idx);
		lua.settable(metaenv_stack_idx);
		lua.pop();

		if (!lua.setfenv(metaprogram_idx))
		{
			ERROR("failed to set environment of metaprogram\n");
			return false;
		}
		
		if (!lua.pcall())
		{
			ERROR("failed to run metaprogram phase 1\n");
			return false;
		}
	}

	// S empty

	// Get lpp and set the proper metaprogram context.
	// The way this is done kinda sucks, but I'll try and clean it up
	// properly when I move all of this into a Metaprogram thing.
	if (!lua.require("lpp"_str))
		return false;
	const s32 lpp_idx = lua.gettop();

	// Save the old context.
	lua.pushstring("context"_str);
	lua.gettable(lpp_idx);
	const s32 prev_context_idx = lua.gettop();

	// ensure the context is restored
	defer 
	{ 
		lua.pushstring("context"_str);
		lua.pushvalue(prev_context_idx);
		lua.settable(lpp_idx);
	};

	// set the new one
	lua.pushstring("context"_str);
	lua.pushlightuserdata(ctx);
	lua.settable(lpp_idx);

	// get metaenv back from the stack so we can use it in phase 2
	// TODO(sushi) rewrite stack logic so we dont have to retrieve this again
	lua.getglobal(lpp_metaenv_stack);
	const s32 metaenv_stack_idx = lua.gettop();
	lua.pushinteger(lua.objlen(metaenv_stack_idx));

	lua.gettable(metaenv_stack_idx);
	const s32 metaenv_table_idx = lua.gettop();

	// retrieve the actual metaenv table from the metaenv 
	// so that processSections may use it 

	lua.pushstring("__metaenv"_str);
	lua.gettable(metaenv_table_idx);

	// move into phase 2

	if (!ctx->metaenv->processSections())
		return false;

	// pop the metaenv stack
	lua.pushinteger(lua.objlen(metaenv_stack_idx));
	lua.pushnil();
	lua.settable(metaenv_stack_idx);

	// write out the final result
	output_stream->write(ctx->metaenv->output->cache.asStr());

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

	if (!lua.loadfile("src/metaenv.lua"))
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
LPP_LUAJIT_FFI_FUNC
MetaprogramBuffer processFile(Lpp* lpp, str path)
{
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
		lpp->contexts.remove(((MetaprogramContext*)m)->list_node);
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
LPP_LUAJIT_FFI_FUNC
void getMetaprogramResult(MetaprogramBuffer mpbuf, void* outbuf)
{
	if (outbuf)
	{
		mpbuf.memhandle->read({(u8*)outbuf, mpbuf.memsize});
	}

	mpbuf.memhandle->close();
}

}


