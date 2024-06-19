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
#include "iro/containers/linked_pool.h"
#include "iro/luastate.h"


#include "source.h"
#include "parser.h"

using namespace iro;

// Use on functions that need to be exposed to luajit's ffi interface.
#define LPP_LUAJIT_FFI_FUNC EXPORT_DYNAMIC

struct Metaprogram;

struct Lpp
{
	LuaState lua;

	DLinkedPool<Metaprogram> metaprograms;
	DLinkedPool<Source> sources;

    b8   init();
	void deinit();

	b8 processStream(str name, io::IO* instream, io::IO* outstream);
}; 

#endif // _lpp_lpp_h
