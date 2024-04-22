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

struct lua_State;

struct Lpp
{
	str input_file_name;
	str output_file_name;

	b8 use_color;
 
	dstr metaprogram;

	Lexer lexer;

	lua_State* L;

    Logger logger;

    Array<Token> tokens;

    io::IO* in;
    io::IO* out;

    b8 init(io::IO* input_stream, io::IO* output_stream, Logger::Verbosity verbosity = Logger::Verbosity::Warn);
	b8 run();
}; 

#endif // _lpp_lpp_h
