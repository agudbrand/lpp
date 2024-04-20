/*
 *
 *  Lexer for C, C++ and lpp
 *
 */

#ifndef _lpp_lex_h
#define _lpp_lex_h

#include "common.h"
#include "array.h"

struct Lpp;

struct Token
{
	enum class Kind
	{
		LuaLine,
		LuaInline,
		LuaBlock,

		Macro,
		MacroArgument,
		
		Subject,
	};

	str raw;

	int line;
	int column;
};

struct Lexer
{
	Token* tokens;
	s64    token_count;
	s64    token_space;

	u64* lua_tokens;
	s64  lua_token_count;
	s64  lua_token_space;

	u8* stream;

	s64 line;
	s64 column;

	Lpp* lpp;

	void init(Lpp* lpp, u8* stream);
	b8   run();
};

#endif // _lpp_lex_h
