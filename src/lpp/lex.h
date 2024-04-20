/*
 *
 */

#ifndef _lpp_lex_h
#define _lpp_lex_h

#include "common.h"
#include "array.h"
#include "logger.h"

struct Lpp;

struct Token
{
	enum class Kind
	{
		Eof,

		LuaLine,
		LuaInline,
		LuaBlock,

		MacroSymbol,
		MacroIdentifier,
		MacroArgumentTupleArg, // (a, b, c, ...)
		MacroArgumentString,   // "..."
		MacroArgumentTable,    // {...}

		// i cannot come up with a better name
		// but this is just any text that is not lpp code
		// eg. if we are preprocessing a C file, this is C code
		Subject, 
	};

	Kind kind;

	str raw;
	str indentation;

	int line;
	int column;

	static str kind_string(Kind kind)
	{
		using enum Kind;
		switch (kind)
		{
			case Eof: return "Eof"_str;

			case LuaLine: return "LuaLine"_str;
			case LuaInline: return "LuaInline"_str;
			case LuaBlock: return "LuaBlock"_str;
			case MacroSymbol: return "MacroSymbol"_str;
			case MacroIdentifier: return "MacroIdentifier"_str;
			case MacroArgumentTupleArg: return "MacroArgumentTupleArg"_str;
			case MacroArgumentString: return "MacroArgumentString"_str;
			case MacroArgumentTable: return "MacroArgumentTable"_str;
			case Subject: return "Subject"_str;
			default: return "*TOKEN KIND WITH NO STRING*"_str;
		}
	}
};

typedef Array<Token> TokenArray;

struct Lexer
{
	TokenArray tokens;

	str filename;

	u8* stream;
	u8* cursor;

	s64 line;
	s64 column;

	str indentation;
	b8  accumulate_indentation;

	Lpp* lpp;

	b8 init(Lpp* lpp, u8* stream, str filename);
	b8 run();

private:

	u8 current();
	b8 at(u8 c);
	b8 eof();
	u8 next();
	b8 next_at(u8 c);
	b8 at_first_identifier_char();
	b8 at_identifier_char();
	b8 at_digit();

	void advance(s32 n = 1);
	void skip_whitespace();

	template<typename... T>
	b8 error_at(s64 line, s64 column, T... args)
	{
		printv(filename, ":", line, ":", column, ": error: ", args..., "\n");
		return false;
	}

	template<typename... T>
	b8 error_here(T... args)
	{
		printv(filename, ":", line, ":", column, ": error: ", args..., "\n");
		return false;
	}
};

struct TokenIterator
{
	Token* curt;
	Token* stop;

	static TokenIterator from(TokenArray& arr)
	{
		return {arr.arr, arr.arr + arr.len()};
	}

	// returns true if the token moves, false if 
	// we're at the end
	b8 next()
	{
		if (curt == stop)
			return false;
		curt += 1;
		return true;
	}

	Token* operator->() { return curt; }
};

#endif // _lpp_lex_h
