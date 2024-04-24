/* ----------------------------------------------
 *
 *	lpp state and interface used throughout the 
 *	project.
 *
 */

#ifndef _lpp_lpp_h
#define _lpp_lpp_h

#include "common.h"
#include "parser.h"
#include "pool.h"

struct lua_State;

struct Lpp
{
    Logger logger;

	lua_State* L;

    b8   init(Logger::Verbosity verbosity = Logger::Verbosity::Warn);
	void deinit();

	b8 create_metaprogram(str name, io::IO* input_stream, io::IO* output_stream);
	b8 run_metaprogram(str name, io::IO* input_stream, io::IO* output_stream);

private:

	io::Memory metaenv_chunk;

	b8 cache_metaenvironment();
	static int cache_writer(lua_State* L, const void* p, size_t sz, void* ud);

	// loads a new metaenv from the cached chunk and leaves it at the top of 
	// the lua stack
	b8 load_metaenvironment();

}; 

#endif // _lpp_lpp_h
