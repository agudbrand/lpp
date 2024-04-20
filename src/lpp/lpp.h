/* ----------------------------------------------
 *
 *	lpp state and interface used throughout the 
 *	project.
 *
 */

#ifndef _lpp_lpp_h
#define _lpp_lpp_h

#include "common.h"

struct lua_State;

struct Lpp
{
	str input_file_name;
	str output_file_name;

	b8 use_color;
 
	dstr metaprogram;

	lua_State* L;

	b8 run();
}; 

#endif // _lpp_lpp_h
