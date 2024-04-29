/* ----------------------------------------------
 *
 *	lpp state and interface used throughout the 
 *	project.
 *
 *  TODO(sushi) add a lpp api function for quitting execution entirely via a longjmp 
 *              or something. When an error occurs in nested metaprograms 
 *              (like via an import macro or sometinhg) it will currently 
 *              report that every single file failed, whne we only want to show info
 *              about the failing file 
 */

#ifndef _lpp_lpp_h
#define _lpp_lpp_h

#include "common.h"
#include "source.h"
#include "parser.h"
#include "containers/pool.h"
#include "luastate.h"

using namespace iro;

struct Metaenvironment;

struct MetaprogramContext
{
	Lpp* lpp;
	Metaenvironment* metaenv;
};

typedef void* Metaprogram;

struct Lpp
{
    Logger logger;

	LuaState lua;

	Pool<MetaprogramContext> contexts;
	Pool<Source> sources;
	Pool<Metaenvironment> metaenvs;

    b8   init(Logger::Verbosity verbosity = Logger::Verbosity::Warn);
	void deinit();

	Metaprogram create_metaprogram(str name, io::IO* input_stream, io::IO* output_stream);
	b8 run_metaprogram(Metaprogram ctx, io::IO* input_stream, io::IO* output_stream);

private:

	io::Memory metaenv_chunk; // i dont think this is necessary but i dont remember why

	b8 cache_metaenvironment();
	static int cache_writer(lua_State* L, const void* p, size_t sz, void* ud);

	// loads a new metaenv from the cached chunk and leaves it at the top of 
	// the lua stack
	b8 load_metaenvironment();

}; 

#endif // _lpp_lpp_h
