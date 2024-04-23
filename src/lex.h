/*
 *
 */

#ifndef _lpp_lex_h
#define _lpp_lex_h

#include "common.h"
#include "array.h"
#include "logger.h"

struct Lpp;

/* ================================================================================================ Token
 */
struct Token
{
	enum class Kind
	{
		Invalid,

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

    s64 source_location;
    s64 length;

    s64 indentation_location;
    s64 indentation_length;

	int line;
	int column;

	static Token invalid() { return {Kind::Invalid}; }

    b8 is_valid() { return kind != Kind::Invalid; }

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

/* ================================================================================================ Lexer
 */
struct Lexer
{
	str stream_name;

    str cursor;
    utf8::Codepoint cursor_codepoint;

	s64 line;
	s64 column;

	str indentation;
	b8  accumulate_indentation;

    io::Memory stream_buffer;
    io::IO* in;

	Token::Kind last_token_kind;

	b8   init(io::IO* input_stream, str stream_name, Logger::Verbosity verbosity = Logger::Verbosity::Warn);
    void deinit();
        
	Token next_token();

    str get_raw(Token& t)
    {
        return {stream_buffer.buffer + t.source_location, t.length};
    }

private:

    Logger logger;

	u32 current();
    u8* currentptr();

	b8 at(u8 c);
	b8 eof();
	b8 at_first_identifier_char();
	b8 at_identifier_char();
	b8 at_digit();

	void advance(s32 n = 1);
	void skip_whitespace();

	template<typename... T>
	Token error_at(s64 line, s64 column, T... args)
	{
        ERROR(stream_name, ":", line, ":", column, ": ", args..., "\n");
		return Token::invalid();
	}

	template<typename... T>
	Token error_here(T... args)
	{
		ERROR(stream_name, ":", line, ":", column, ": error: ", args..., "\n");
		return Token::invalid();
	}
};

/* ================================================================================================ TokenIterator
 */
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
