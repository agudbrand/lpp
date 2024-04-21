/*
 *  JSON parser
 *
 */

#ifndef _lpp_json_parser_h
#define _lpp_json_parser_h

#include "lex.h"
#include "types.h"
#include "unicode.h"

namespace json
{

/* ================================================================================================ json::Parser
 */
struct Parser
{
	using TKind = Token::Kind;
	using VKind = Value::Kind;

	typedef SList<Value> ValueStack;

	Lexer lexer;
	JSON* json;
	Token curt;

	ValueStack value_stack;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	// Initializes the parser with the stream to parse
	// and a JSON object to fill with information.
	// The given JSON is assumed to not be initialized and to contain
	// no information!
	// 'stream_name' is used in error reporting
	b8   init(str stream, JSON* json, str stream_name);
	void deinit();

	// Returns a pointer to a JSON object on success,
	// nullptr otherwise.
	b8 start();

private:

	template<typename... T>
	b8 error_here(T... args)
	{
		printv("json.parser: ", lexer.stream_name, ":", curt.line, ":", curt.column, ": error: ", args..., "\n");
		return false;
	}

	template<typename... T>
	b8 error_at(s64 line, s64 column, T... args)
	{
		printv("json.parser: ", lexer.stream_name, ":", line, ":", column, ": error: ", args..., "\n");
		return false;
	}

	template<typename... T>
	b8 error_no_location(T... args)
	{
		printv("json.parse: error: ", args..., "\n");
		return false;
	}

	void next_token();

	b8 at(TKind kind);

	b8 value();
	b8 object();
	b8 array();
};

} // namespace json

#endif // _lpp_json_parser_h
