#include "lex.h"
#include "logger.h"

#include "ctype.h"
#include "assert.h"

// ==============================================================================================================
/* ------------------------------------------------------------------------------------------------ Lexer helpers
 */
u32 Lexer::current()    { return cursor_codepoint; }
u8* Lexer::currentptr() { return cursor.bytes - cursor_codepoint.advance; }

b8 Lexer::at(u8 c) { return current() == c; }
b8 Lexer::eof()    { return at(0) || !cursor_codepoint.is_valid(); }

b8 Lexer::at_first_identifier_char() { return isalpha(current()) || at('_'); }
b8 Lexer::at_identifier_char() { return at_first_identifier_char() || isdigit(current()); }

void Lexer::advance(s32 n)
{
    auto read_stream_if_needed = [this]()
    {
        if (source->cache.at_end() ||
            cursor.isempty())
        {
            u8* ptr = source->cache.reserve(128);
            s64 bytes_read = in->read({ptr, 128});
            if (!bytes_read)
            {
                cursor_codepoint = utf8::Codepoint::invalid();
                error_here("failed to read more bytes from input stream");
				longjmp(err_handler, 0);
            }
            source->cache.commit(bytes_read);
            cursor.bytes = ptr;
            cursor.len = 128;
        }
    };

    read_stream_if_needed();

	for (s32 i = 0; i < n; i++)
	{
        read_stream_if_needed();
        cursor_codepoint = cursor.advance();

        if (!cursor_codepoint)
        {
            error_here("encountered invalid codepoint!");
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

void Lexer::skip_whitespace()
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

/* ------------------------------------------------------------------------------------------------ Lexer::next_token
 */
b8 Lexer::run()
{
	using enum Token::Kind;

	TRACE("run\n");

	for (;;)
	{
		init_curt();
		skip_whitespace();

		while (not at('@') and
			   not at('$') and
			   not eof())
			advance();

		finish_curt(Document);

		if (eof())
		{
			init_curt();
			advance();
			finish_curt(Eof);
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
					init_curt();

					for (;;)
					{
						if ((advance(), at('$')) and
							(advance(), at('$')) and
							(advance(), at('$')))
						{
							advance();
							finish_curt(LuaBlock, -3);
							break;
						}

						if (eof())
							return error_at_token(curt, "unexpected eof while consuming lua block");
					}
				}
				else
				{
					// dont handle this case for now 
					return error_here("two '$' in a row is currently unrecognized");
				}
			}
			else if (at('('))
			{
				advance();
				init_curt();

				s64 nesting = 1;
				for (;;)
				{
					advance();
					if (eof()) 
						return error_at_token(curt, "unexpected eof while consuming inline lua expression");
					
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

				finish_curt(LuaInline);
				advance();
			}
			else
			{
				init_curt();
				while (not at('\n') and not eof())
					advance();

				finish_curt(LuaLine);

				if (not eof())
					advance();

				// remove whitespace before lua lines to preserve formatting 
				// NOTE(sushi) this is a large reason why the lexer is not token-stream based anymore!
				if (tokens.len() != 0)
				{
					Token* last = tokens.arr + tokens.len() - 2;
					str raw = source->get_str(last->source_location, last->length);
					while (isspace(raw.bytes[last->length-1]) && raw.bytes[last->length-1] != '\n')
						last->length -= 1;
				}
			}
		}
		else if (at('@'))
		{
			init_curt();
			advance();
			finish_curt(MacroSymbol);

			skip_whitespace();
			init_curt();

			if (not at_first_identifier_char())
				return error_here("expected an identifier of a macro after '@'");
			
			// NOTE(sushi) allow '.' so that we can index tables through macros, eg.
			//             $ local lib = {}
			//             $ lib.func = function() print("hi!") end
			//
			//             @lib.func()
			while (at_identifier_char() or at('.'))
				advance();
			
			finish_curt(MacroIdentifier);
			skip_whitespace();

			switch (current())
			{
			case '(':
				advance();
				skip_whitespace();

				if (at(')'))
				{
					// dont create a token for empty macro args
					advance();
					init_curt();
					break;
				}

				for (;;)
				{
					init_curt();

					while (not at(',') and 
						   not at(')') and 
						   not eof())
						advance();

					if (eof())
						return error_at_token(curt, "unexpected end of file while consuming macro arguments");

					finish_curt(MacroArgumentTupleArg);

					if (at(')'))
						break;

					advance();
					skip_whitespace();
				}

				advance();
				init_curt();
				break;

			case '"':
				advance();
				init_curt();

				for (;;)
				{
					advance();
					if (eof())
						return error_at_token(curt, "unexpected end of file while consuming macro string argument");

					if (at('"'))
						break;
				}
				finish_curt(MacroArgumentString);
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

/* ------------------------------------------------------------------------------------------------ Lexer::reset_token
 */
void Lexer::init_curt()
{
	curt.kind = Token::Kind::Eof;
	curt.source_location = currentptr() - source->cache.buffer;
}

/* ------------------------------------------------------------------------------------------------ Lexer::finish_token
 */
void Lexer::finish_curt(Token::Kind kind, s32 len_offset)
{
	s32 len = (currentptr() - source->cache.buffer) - curt.source_location;
	if (len != 0)
	{
		curt.kind = kind;
		curt.length = len + len_offset;
		tokens.push(curt);
	}
}

/* ------------------------------------------------------------------------------------------------ Lexer::consume_document_text
 */
b8 Lexer::consume_document_text()
{
	// init_curt();

	while (not at('@') and
		   not at('$') and 
		   not eof())
		advance();

	finish_curt(Document);

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lexer::consume_lua_code
 */
b8 Lexer::consume_lua_code()
{
	advance();
	if (at('$'))
	{
		advance();
		if (at('$'))
		{
			advance();
			init_curt();
			for (;;)
			{
				if ((advance(), at('$')) and
					(advance(), at('$')) and
					(advance(), at('$')))
				{
					advance();
					finish_curt(LuaBlock, -3);
					return true;
				}

				if (eof())
					return error_at_token(curt, "unexpected eof while consuming lua block");
			}
		}
		else
		{
			// dont handle this case for now
			// TODO(sushi) decide what should be done in this case later on
			return error_here("two of '$' in a row is unrecognized!");
		}
	}
	else if (at('('))
	{
		advance();
		init_curt();

		s64 nesting = 1;

		for (;;)
		{
			advance();
			if      (eof()) break;
			else if (at('(')) nesting += 1;
			else if (at(')'))
			{
				if (nesting == 1)
					break;
				else
					nesting -= 1;
			}
		}

		if (eof())
			return error_at_token(curt, "unexpected eof while consuming inline lua");

		advance();
		finish_curt(LuaInline, -1);
		return true;
	}
	else
	{
		init_curt();
		while (not at('\n') and not eof())
			advance();

		finish_curt(LuaLine);

		if (not eof())
			advance();

		return true;
	}

}

/* ------------------------------------------------------------------------------------------------ Lexer::consume_macro_identifier
 */
b8 Lexer::consume_macro_identifier()
{
	init_curt();

	if (not at_first_identifier_char())
		return error_here("expected an identifier of a macro after '@'");

	while (at_identifier_char())
		advance();

	finish_curt(MacroIdentifier);

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lexer::consume_macro_tuple_argument
 */
b8 Lexer::consume_macro_tuple_argument()
{
	if (at(')'))
	{
		advance();
		return consume_document_text();
	}

	init_curt();

	while (not at(',') and 
		   not at(')') and 
		   not eof())
		advance();

	if (eof())
		return error_at_token(curt, "unexpected eof while consuming macro tuple arguments");

	finish_curt(MacroArgumentTupleArg);

	return true;
}
