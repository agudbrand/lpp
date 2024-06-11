#include "parser.h"

#include "ctype.h"

static Logger logger = Logger::create("lpp.parser"_str, Logger::Verbosity::Notice);

/* ------------------------------------------------------------------------------------------------ Parser::init
 */
b8 Parser::init(
		Source* src,
		io::IO* instream, 
		io::IO* outstream)
{
	assert(instream && outstream);

	TRACE("initializing on stream '", src->name, "'\n");

	tokens = Array<Token>::create();
	in = instream;
	out = outstream;
	source = src;

	if (!lexer.init(in, src))
		return false;

	if (setjmp(lexer.err_handler))
	{
		lexer.deinit();
		return false;
	}

	if (!lexer.run())
		return false;

	curt = lexer.tokens.arr-1;

	src->cacheLineOffsets();
	return true;
}

/* ------------------------------------------------------------------------------------------------ Parser::init
 */
void Parser::deinit()
{
	lexer.deinit();
	*this = {};
}

/* ------------------------------------------------------------------------------------------------ Parser::run
 */
b8 Parser::run()
{
	TRACE("begin\n");

	nextToken();

	for (;;)
	{
		using enum Token::Kind;

		if (at(Eof))
		{
			TRACE("encountered EOF\n");
			break;
		}

		switch (curt->kind)
		{
			case Invalid:
				return false;

			case Document:
				TRACE("placing document text: '", io::SanitizeControlCharacters(getRaw()), "'\n");
				writeOut("__metaenv.doc("_str, curt->loc, ",\""_str);
				// sanitize the document text's control characters into lua's 
				// represenatations of them
				for (u8 c : getRaw())
				{
					if (iscntrl(c))
						writeOut("\\"_str, c);
					else if (c == '"')
						writeOut("\\\""_str);
					else if (c == '\\')
						writeOut("\\\\"_str);
					else 
						writeOut((char)c);
				}

				writeOut("\")\n"_str);
				nextToken();
				break;

			case LuaBlock:
				TRACE("placing lua block: '", io::SanitizeControlCharacters(getRaw()), "'\n");
				writeOut(getRaw(), "\n");
				nextToken();
				break;

			case LuaLine:
				TRACE("placing lua line: '", io::SanitizeControlCharacters(getRaw()), "'\n");
				writeOut(getRaw(), "\n");
				nextToken();
				break;

			case LuaInline:
				TRACE("placing lua inline: '", io::SanitizeControlCharacters(getRaw()), "'\n");
				writeOut("__metaenv.val("_str, curt->loc, ",", getRaw(), ")\n"_str);
				nextToken();
				break;

			case MacroSymbol:
				TRACE("encountered macro symbol\n");
				writeOut(
					"__metaenv.macro("_str, curt->loc, ",\"", 
					source->getStr(curt->macro_indent_loc, curt->macro_indent_len), 
					"\",");

				nextToken(); // identifier
				writeOut(getRaw());

				nextToken();
				if (at(MacroArgumentTupleArg))
				{
					for (;;)
					{
						writeOut(',', 
							"__metaenv.MacroPart.new(",
							'"', source->name, '"', ',',
							curt->loc,              ',',
							curt->loc + curt->len,  ',',
							'"', getRaw(), '"', ')');
						nextToken();
						if (not at(MacroArgumentTupleArg))
							break;
						// writeOut(',');
					}
				}
				else if (at(MacroArgumentString))
				{
					writeOut(',', '"', getRaw(), '"');
					nextToken();
				}

				writeOut(")\n"_str);
				break;
		}
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ Parser::nextToken
 */
b8 Parser::nextToken()
{
	curt += 1;
	return true;
}

/* ------------------------------------------------------------------------------------------------ Parser::at
 */
b8 Parser::at(Token::Kind kind)
{
	return curt->kind == kind;
}

/* ------------------------------------------------------------------------------------------------ Parser::getRaw
 */
str Parser::getRaw()
{
	return curt->getRaw(source);
}
