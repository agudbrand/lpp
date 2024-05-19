#include "lex.h"

#include "iro/logger.h"

#include "ctype.h"
#include "assert.h"

// ==============================================================================================================
/* ------------------------------------------------------------------------------------------------ Lexer helpers
 */
u32 Lexer::current()    { return cursor_codepoint; }
u8* Lexer::currentptr() { return cursor.bytes - cursor_codepoint.advance; }

b8 Lexer::at(u8 c) { return current() == c; }
b8 Lexer::eof()    { return at(0) || isnil(cursor_codepoint); }

b8 Lexer::atFirstIdentifierChar() { return isalpha(current()) || at('_'); }
b8 Lexer::atIdentifierChar() { return atFirstIdentifierChar() || isdigit(current()); }

void Lexer::advance(s32 n)
{
    auto readStreamIfNeeded = [this]()
    {
        if (source->cache.atEnd() ||
            cursor.isEmpty())
        {
        	Bytes bytes = source->cache.reserve(128);
            s64 bytes_read = in->read(bytes);
            if (!bytes_read)
            {
                cursor_codepoint = nil;
                errorHere("failed to read more bytes from input stream");
				longjmp(err_handler, 0);
            }
            source->cache.commit(bytes_read);
			cursor = str::from(bytes.ptr, bytes_read);
        }
    };

    readStreamIfNeeded();

	for (s32 i = 0; i < n; i++)
	{
        readStreamIfNeeded();
        cursor_codepoint = cursor.advance();

        if (!cursor_codepoint)
        {
            errorHere("encountered invalid codepoint!");
            longjmp(err_handler, 0);
        }

		if (at('\n'))
		{
			in_indentation = true;
		}
		else if (in_indentation)
		{
			if (not isspace(current()))
				in_indentation = false;
		}
        
	}
}

void Lexer::skipWhitespace()
{
	while (isspace(current()))
		advance();
}

/* ------------------------------------------------------------------------------------------------ Lexer::init
 */
b8 Lexer::init(io::IO* input_stream, Source* src, Logger::Verbosity verbosity)
{
    assert(input_stream);

    logger.init("lpp.lexer"_str, verbosity);

    TRACE("initializing with input stream '", src->name, "'\n");
	SCOPED_INDENT;

	tokens = TokenArray::create();
    in = input_stream;
	source = src;

    advance();

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lexer::deinit
 */
void Lexer::deinit()
{
	tokens.destroy();
	*this = {};
}

/* ------------------------------------------------------------------------------------------------ Lexer::nextToken
 */
b8 Lexer::run()
{
	using enum Token::Kind;

	TRACE("run\n");

	for (;;)
	{
		initCurt();
		skipWhitespace();

		while (not at('@') and
			   not at('$') and
			   not eof())
			advance();

		finishCurt(Document);

		if (eof())
		{
			initCurt();
			advance();
			finishCurt(Eof);
			return true;
		}

		if (at('$'))
		{
			advance();
			if (at('$'))
			{
				advance();
				if (at('$'))
				{
					advance();
					initCurt();

					for (;;)
					{
						if ((advance(), at('$')) and
							(advance(), at('$')) and
							(advance(), at('$')))
						{
							advance();
							finishCurt(LuaBlock, -3);
							break;
						}

						if (eof())
							return errorAtToken(curt, "unexpected eof while consuming lua block");
					}
				}
				else
				{
					// dont handle this case for now 
					return errorHere("two '$' in a row is currently unrecognized");
				}
			}
			else if (at('('))
			{
				advance();
				initCurt();

				s64 nesting = 1;
				for (;;)
				{
					advance();
					if (eof()) 
						return errorAtToken(curt, "unexpected eof while consuming inline lua expression");
					
					if (at('(')) 
						nesting += 1;
					else if (at(')'))
					{
						if (nesting == 1)
							break;
						else 
							nesting -= 1;
					}
				}

				finishCurt(LuaInline);
				advance();
			}
			else
			{
				initCurt();
				while (not at('\n') and not eof())
					advance();

				finishCurt(LuaLine);

				if (not eof())
					advance();

				// remove whitespace before lua lines to preserve formatting 
				// NOTE(sushi) this is a large reason why the lexer is not token-stream based anymore!
				if (tokens.len() != 0)
				{
					Token* last = tokens.arr + tokens.len() - 2;
					str raw = source->getStr(last->source_location, last->length);
					while (isspace(raw.bytes[last->length-1]) && raw.bytes[last->length-1] != '\n')
						last->length -= 1;
				}
			}
		}
		else if (at('@'))
		{
			initCurt();
			advance();
			finishCurt(MacroSymbol);

			skipWhitespace();
			initCurt();

			if (not atFirstIdentifierChar())
				return errorHere("expected an identifier of a macro after '@'");
			
			// NOTE(sushi) allow '.' so that we can index tables through macros, eg.
			//             $ local lib = {}
			//             $ lib.func = function() print("hi!") end
			//
			//             @lib.func()
			while (atIdentifierChar() or at('.'))
				advance();
			
			finishCurt(MacroIdentifier);
			skipWhitespace();

			switch (current())
			{
			case '(':
				advance();
				skipWhitespace();

				if (at(')'))
				{
					// dont create a token for empty macro args
					advance();
					initCurt();
					break;
				}

				for (;;)
				{
					initCurt();

					while (not at(',') and 
						   not at(')') and 
						   not eof())
						advance();

					if (eof())
						return errorAtToken(curt, "unexpected end of file while consuming macro arguments");

					finishCurt(MacroArgumentTupleArg);

					if (at(')'))
						break;

					advance();
					skipWhitespace();
				}

				advance();
				initCurt();
				break;

			case '"':
				advance();
				initCurt();

				for (;;)
				{
					advance();
					if (eof())
						return errorAtToken(curt, "unexpected end of file while consuming macro string argument");

					if (at('"'))
						break;
				}
				finishCurt(MacroArgumentString);
				advance();
				break;
			}
		}
		else
		{
			
		}
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lexer::resetToken
 */
void Lexer::initCurt()
{
	curt.kind = Token::Kind::Eof;
	curt.source_location = currentptr() - source->cache.buffer;
}

/* ------------------------------------------------------------------------------------------------ Lexer::finishToken
 */
void Lexer::finishCurt(Token::Kind kind, s32 len_offset)
{
	s32 len = (currentptr() - source->cache.buffer) - curt.source_location;
	if (len != 0)
	{
		curt.kind = kind;
		curt.length = len + len_offset;
		tokens.push(curt);
	}
}


