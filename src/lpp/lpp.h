/* ----------------------------------------------
 *
 *	lpp state and interface used throughout the 
 *	project.
 *
 */

#ifndef _lpp_lpp_h
#define _lpp_lpp_h

#include "common.h"
#include "lex.h"

typedef struct lua_State lua_State;

typedef struct lppContext
{
	u8* input_file_name;
	u8* output_file_name;

	b8 use_color;
	b8 cpp; // if we need to consider cpp in lexing
 
	dstr metaprogram;

	Lexer lexer;

	lua_State* L;
} lppContext; 

void lpp_run(lppContext* lpp);

/* ----------------------------------------------
 *  Functions for reporting stuff
 */

// does not return
void fatal_error(lppContext* lpp, s64 line, s64 column, const char* fmt, ...);
void error(lppContext* lpp, s64 line, s64 column, const char* fmt, ...);
void warning(lppContext* lpp, s64 line, s64 column, const char* fmt, ...); 
  
#endif // _lpp_lpp_h
