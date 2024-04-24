#include "parser.h"

#include "ctype.h"

/* ------------------------------------------------------------------------------------------------ Parser::init
 */
b8 Parser::init(
		io::IO* input_stream, 
		str     stream_name, 
		io::IO* output_stream, 
		Logger::Verbosity verbosity)
{
	assert(input_stream && output_stream);

	logger.init("lpp.parser"_str, verbosity);

	INFO("initializing on stream '", stream_name, "'\n");

	tokens = Array<Token>::create();
	in = input_stream;

	if (!lexer.init(in, stream_name, verbosity))
		return false;

	return true;
}

/* ------------------------------------------------------------------------------------------------ Parser::init
 */
void Parser::deinit()
{
	tokens.destroy();
	in = nullptr;
	lexer.deinit();
}

/* ------------------------------------------------------------------------------------------------ Parser::run
 */
b8 Parser::run()
{
	INFO("begin\n");

	DEBUG("opening metaprogram buffer\n");
	metaprogram.open();

	// wrap call to next token so i dont have to write out this whole thing every single time
	// this could fail if the lexer encounters unicode that is invalid and stuff
	// should probably handle this better later on? not sure if it will ever happen anyhow

	next_token();

	for (;;)
	{
		using enum Token::Kind;

		if (at(Eof))
			break;

		switch (curt.kind)
		{
			case Invalid:
				return false;

			case Subject:
				TRACE("placing document text: '", io::fmt::SanitizeControlCharacters(get_raw()), "'\n");
				write_metaprogram("__SUBJECT(\""_str);
				// sanitize the document text's control characters into lua's represenatations of them
				for (u8 c : get_raw())
				{
					if (iscntrl(c))
						write_metaprogram("\\"_str, c);
					else if (c == '"')
						write_metaprogram("\\\""_str);
					else if (c == '\\')
						write_metaprogram("\\\\"_str);
					else 
						write_metaprogram((char)c);
				}

				write_metaprogram("\")\n"_str);
				next_token();
				break;

			case LuaBlock:
				TRACE("placing lua block: '", io::fmt::SanitizeControlCharacters(get_raw()), "'\n");
				write_metaprogram(get_raw(), "\n");
				next_token();
				break;

			case LuaLine:
				TRACE("placing lua line: '", io::fmt::SanitizeControlCharacters(get_raw()), "'\n");
				write_metaprogram(get_raw(), "\n");
				next_token();
				break;

			case LuaInline:
				TRACE("placing lua inline: '", io::fmt::SanitizeControlCharacters(get_raw()), "'\n");
				write_metaprogram("__SUBJECT(__VAL("_str, get_raw(), "))\n"_str);
				next_token();
				break;

			case MacroSymbol:
				TRACE("encountered macro symbol\n");
				write_metaprogram(
						"__SET_MACRO_TOKEN_INDEX("_str, tokens.len(), ")\n",
						"__SUBJECT(__MACRO("_str);

				next_token(); // identifier
				write_metaprogram(get_raw());

				next_token();
				if (at(MacroArgumentTupleArg))
				{
					write_metaprogram('(');
					for (;;)
					{
						write_metaprogram('"', get_raw(), '"');
						next_token();
						if (at(Eof) or not at(MacroArgumentTupleArg))
							break;
						write_metaprogram(',');
					}
					write_metaprogram(')');
				}

				write_metaprogram("))\n"_str);
				break;
		}
	}

	write_metaprogram("return __FINAL()\n");

	return true;
}

/* ------------------------------------------------------------------------------------------------ Parser::next_token
 */
b8 Parser::next_token()
{
	if (setjmp(lexer.err_handler))
		longjmp(err_handler, 0);

	curt = lexer.next_token();
	if (!curt.is_valid())
		return false;
	tokens.push(curt);
	return true;
}

/* ------------------------------------------------------------------------------------------------ Parser::at
 */
b8 Parser::at(Token::Kind kind)
{
	return curt.kind == kind;
}

/* ------------------------------------------------------------------------------------------------ Parser::get_raw
 */
str Parser::get_raw()
{
	return lexer.get_raw(curt);
}
