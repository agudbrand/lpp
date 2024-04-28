#include "parser.h"

#include "ctype.h"

/* ------------------------------------------------------------------------------------------------ Parser::init
 */
b8 Parser::init(
		Source* src,
		io::IO* instream, 
		io::IO* outstream, 
		Logger::Verbosity verbosity)
{
	assert(instream && outstream);

	logger.init("lpp.parser"_str, verbosity);

	TRACE("initializing on stream '", src->name, "'\n");

	tokens = Array<Token>::create();
	in = instream;
	out = outstream;
	source = src;

	if (!lexer.init(in, src, verbosity))
		return false;

	return true;
}

/* ------------------------------------------------------------------------------------------------ Parser::init
 */
void Parser::deinit()
{
	tokens.destroy();
	lexer.deinit();
	*this = {};
}

/* ------------------------------------------------------------------------------------------------ Parser::run
 */
b8 Parser::run()
{
	TRACE("begin\n");

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

			case Document:
				TRACE("placing document text: '", io::SanitizeControlCharacters(get_raw()), "'\n");
				write_out("__metaenv.doc(\""_str);
				// sanitize the document text's control characters into lua's represenatations of them
				for (u8 c : get_raw())
				{
					if (iscntrl(c))
						write_out("\\"_str, c);
					else if (c == '"')
						write_out("\\\""_str);
					else if (c == '\\')
						write_out("\\\\"_str);
					else 
						write_out((char)c);
				}

				write_out("\")\n"_str);
				next_token();
				break;

			case LuaBlock:
				TRACE("placing lua block: '", io::SanitizeControlCharacters(get_raw()), "'\n");
				write_out(get_raw(), "\n");
				next_token();
				break;

			case LuaLine:
				TRACE("placing lua line: '", io::SanitizeControlCharacters(get_raw()), "'\n");
				write_out(get_raw(), "\n");
				next_token();
				break;

			case LuaInline:
				TRACE("placing lua inline: '", io::SanitizeControlCharacters(get_raw()), "'\n");
				write_out("__metaenv.val("_str, get_raw(), ")\n"_str);
				next_token();
				break;

			case MacroSymbol:
				TRACE("encountered macro symbol\n");
				write_out("__metaenv.macro("_str);

				next_token(); // identifier
				write_out(get_raw());

				next_token();
				if (at(MacroArgumentTupleArg))
				{
					for (;;)
					{
						write_out(',', '"', get_raw(), '"');
						next_token();
						if (at(Eof) or not at(MacroArgumentTupleArg))
							break;
						write_out(',');
					}
				}
				else if (at(MacroArgumentString))
				{
					write_out(',', '"', get_raw(), '"');
					next_token();
				}

				write_out(")\n"_str);
				break;
		}
	}

	write_out("return __metaenv.final()\n");

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
	return curt.get_raw(source);
}
