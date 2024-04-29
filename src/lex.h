/*
 *
 */

#ifndef _lpp_lex_h
#define _lpp_lex_h

#include "common.h"

#include "containers/array.h"
#include "logger.h"
#include "source.h"

#include "csetjmp"

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

		// Any text that lpp is preprocessing. Eg. if we 
		// are preprocessing a C file, this is C code.
		Document, 
	};

	Kind kind;

    s32 source_location;
	s32 length;

	static Token invalid() { return {Kind::Invalid}; }

    b8 is_valid() { return kind != Kind::Invalid; }

	// retrieve the raw string this token encompasses from the given Source
	str get_raw(Source* src) { return src->get_str(source_location, length); }

	static str kind_string(Kind kind)
	{
		using enum Kind;
		switch (kind)
		{
			case Eof: return "Eof"_str;

			case LuaLine:               return "LuaLine"_str;
			case LuaInline:             return "LuaInline"_str;
			case LuaBlock:              return "LuaBlock"_str;
			case MacroSymbol:           return "MacroSymbol"_str;
			case MacroIdentifier:       return "MacroIdentifier"_str;
			case MacroArgumentTupleArg: return "MacroArgumentTupleArg"_str;
			case MacroArgumentString:   return "MacroArgumentString"_str;
			case Document:              return "Document"_str;
			default: return "*TOKEN KIND WITH NO STRING*"_str;
		}
	}
};


/* ================================================================================================ Lexer
 *
 *  TODO(sushi) try reworking the lexer to give a token
 *	            stream again instead of caching them into an array
 * 	            to save on memory. Making it a stream complicates the 
 *	            internal logic too much at the moment so I'm just 
 *	            going to avoid it for now.
 *	            It's not a huge issue, though, because Tokens are quite small, and with how lpp 
 *	            works we shouldn't be making too many tokens to begin with.
 */
struct Lexer
{
	using enum Token::Kind;
	typedef Array<Token> TokenArray;

    str cursor;
    utf8::Codepoint cursor_codepoint;

	Source* source;
    io::IO* in;

	TokenArray tokens;

	b8 in_indentation;

	jmp_buf err_handler; // this is 200 bytes !!!

	b8   init(io::IO* input_stream, Source* src, Logger::Verbosity verbosity = Logger::Verbosity::Warn);
    void deinit();
        
	b8 run();

private:

    Logger logger;

	Token curt;

	void init_curt();
	void finish_curt(Token::Kind kind, s32 len_offset = 0);

	b8 consume_document_text();
	b8 consume_lua_code(); // $
	b8 consume_macro_identifier(); // @
	b8 consume_macro_tuple_argument();

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
	b8 error_at(s32 line, s32 column, T... args)
	{
        ERROR(source->name, ":", line, ":", column, ": ", args..., "\n");
		return false;
	}

	template<typename... T>
	b8 error_at_token(Token& t, T... args)
	{
		Source::Loc loc = source->get_loc(t.source_location);
		return error_at(loc.line, loc.column, args...);
	}

	template<typename... T>
	b8 error_here(T... args)
	{
		Source::Loc loc = source->get_loc(cursor.bytes - source->cache.buffer);
		return error_at(loc.line, loc.column, args...);
	}

};

#endif // _lpp_lex_h
