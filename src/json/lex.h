/*
 *  Incremental JSON lexer
 *
 */

#ifndef _lpp_json_lex_h
#define _lpp_json_lex_h

#include "common.h"
#include "flags.h"
#include "unicode.h"
#include "logger.h"

namespace json
{

/* ================================================================================================ json::Token
 */
struct Token
{
	enum class Kind
	{
		Invalid,

		Eof,

		LeftSquare,
		RightSquare,
		LeftBrace,
		RightBrace,
		Colon,
		Comma,

		Whitespace,

		True,
		False,
		Null,
		Number,
		String,
	};

	Kind kind;

	u32 line;
	u32 column;

	str raw;

	static Token invalid() { return {Kind::Invalid}; }
    b8 is_valid() { return kind != Kind::Invalid; }
};

/* ================================================================================================ json::Lexer
 */
struct Lexer
{
	str stream_name;

    io::Memory stream_buffer;
    io::IO* in;

	str cursor;
	utf8::Codepoint cursor_codepoint;

	u32 line;
	u32 column;

	enum class Flag
	{
		ReturnWhitespaceTokens,
	};
	typedef Flags<Flag> Flags;

	Flags flags;

	b8   init(io::IO* input_stream, str stream_name, Logger::Verbosity verbosity = Logger::Verbosity::Warn);
	void deinit();

	Token next_token();

private:

    Logger logger;

	u32 current();

	b8 at(u8 c);
	b8 eof();
	b8 at_first_identifier_char();
	b8 at_identifier_char();
	b8 at_digit();
	b8 at_whitespace();

	void advance(s32 n = 1);
	void skip_whitespace();

	template<io::Formattable... T>
	Token error_here(T... args)
	{
		ERROR(stream_name, ":"_str, line, ":"_str, column, ": "_str, args..., "\n");
		return Token::invalid();
	}

	template<io::Formattable... T>
	Token error_at(u32 line, u32 column, T... args)
	{
		ERROR(stream_name, ":"_str, line, ":"_str, column, ": "_str, args..., "\n");
		return Token::invalid();
	}
};

} // namespace json

#endif // _lpp_json_lex_h
