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

#include "iro/common.h"
#include "iro/containers/pool.h"
#include "iro/containers/list.h"
#include "iro/luastate.h"

#include "source.h"
#include "parser.h"

using namespace iro;

// Use on functions that need to be exposed to luajit's ffi interface.
#define LPP_LUAJIT_FFI_FUNC EXPORT_DYNAMIC

struct Metaenvironment;

struct MetaprogramContext
{
	Lpp* lpp;
	Metaenvironment* metaenv;
	DList<MetaprogramContext>::Node* list_node;
};

typedef void* Metaprogram;

struct Lpp
{
	LuaState lua;

	// TODO(sushi) fix up linked_pool so that it can properly replace all of this
	Pool<MetaprogramContext> context_pool;
	DList<MetaprogramContext> contexts;

	Pool<Source> source_pool;
	SList<Source> sources;

	Pool<Metaenvironment> metaenv_pool;
	SList<Metaenvironment> metaenvs;

    b8   init();
	void deinit();

	Metaprogram createMetaprogram(str name, io::IO* input_stream, io::IO* output_stream);
	b8 runMetaprogram(Metaprogram ctx, io::IO* input_stream, io::IO* output_stream);

private:

	io::Memory metaenv_chunk; // i dont think this is necessary but i dont remember why

	b8 cacheMetaenvironment();
	static int cacheWriter(lua_State* L, const void* p, size_t sz, void* ud);

	// loads a new metaenv from the cached chunk and leaves it at the top of 
	// the lua stack
	b8 loadMetaenvironment();
}; 

#endif // _lpp_lpp_h
