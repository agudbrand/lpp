/*
 *
 */

#ifndef _lpp_lex_h
#define _lpp_lex_h

#include "iro/common.h"
#include "iro/containers/array.h"
#include "iro/logger.h"

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
	str macro_indentation;

	static Token invalid() { return {Kind::Invalid}; }

    b8 isValid() { return kind != Kind::Invalid; }

	// retrieve the raw string this token encompasses from the given Source
	str getRaw(Source* src) { return src->getStr(source_location, length); }

	static str getKindString(Kind kind)
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

	void initCurt();
	void finishCurt(Token::Kind kind, s32 len_offset = 0);

	u32 current();
    u8* currentptr();

	b8 at(u8 c);
	b8 eof();
	b8 atFirstIdentifierChar();
	b8 atIdentifierChar();
	b8 atDigit();

	void advance(s32 n = 1);
	void skipWhitespace();

	template<typename... T>
	b8 errorAt(s32 line, s32 column, T... args)
	{
        ERROR(source->name, ":", line, ":", column, ": ", args..., "\n");
		return false;
	}

	template<typename... T>
	b8 errorAtToken(Token& t, T... args)
	{
		Source::Loc loc = source->getLoc(t.source_location);
		return errorAt(loc.line, loc.column, args...);
	}

	template<typename... T>
	b8 errorHere(T... args)
	{
		Source::Loc loc = source->getLoc(cursor.bytes - source->cache.buffer);
		return errorAt(loc.line, loc.column, args...);
	}

};

#endif // _lpp_lex_h
